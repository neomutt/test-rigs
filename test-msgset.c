#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "imap/adata.h"
#include "imap/edata.h"
#include "imap/private.h"
#include "mx.h"
#include "sort.h"

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "sort", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT, SORT_DATE, 0, NULL, },
  { NULL },
};

void mx_alloc_memory(struct Mailbox *m)
{
  const int grow = 25;
  size_t s = MAX(sizeof(struct Email *), sizeof(int));

  if (((m->email_max + grow) * s) < (m->email_max * s))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  m->email_max += grow;
  if (m->emails)
  {
    mutt_mem_realloc(&m->emails, m->email_max * sizeof(struct Email *));
  }
  else
  {
    m->emails = mutt_mem_calloc(m->email_max, sizeof(struct Email *));
  }
  for (int i = m->email_max - grow; i < m->email_max; i++)
  {
    if (i < m->email_max)
    {
      m->emails[i] = NULL;
    }
  }
}
void nm_edata_free(void **ptr)
{
}

struct ImapEmailData *imap_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}

struct ImapAccountData *imap_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_IMAP) || !m->account)
    return NULL;
  return m->account->adata;
}

static int compare_lines(const void *a, const void *b)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  return mutt_numeric_cmp(ea->lines, eb->lines);
}

static int compare_uid(const void *a, const void *b)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  const unsigned int ua = imap_edata_get((struct Email *) ea)->uid;
  const unsigned int ub = imap_edata_get((struct Email *) eb)->uid;

  return mutt_numeric_cmp(ua, ub);
}

int imap_exec(struct ImapAccountData *adata, const char *cmdstr, ImapCmdFlags flags)
{
  printf("%s\n\n", cmdstr);
  return 0;
}

static int make_msg_set(struct Mailbox *m, struct Buffer *buf, enum MessageType flag, bool changed, bool invert, int *pos)
{
  int count = 0;             /* number of messages in message set */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;

  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  for (n = *pos; (n < m->msg_count) && (buf_len(buf) < IMAP_MAX_CMDLEN); n++)
  {
    struct Email *e = m->emails[n];
    if (!e)
      break;
    bool match = false; /* whether current message matches flag condition */
    /* don't include pending expunged messages.
     *
     * TODO: can we unset active in cmd_parse_expunge() and
     * cmd_parse_vanished() instead of checking for index != INT_MAX. */
    if (e->active && (e->index != INT_MAX))
    {
      switch (flag)
      {
        case MUTT_DELETED:
          if (e->deleted != imap_edata_get(e)->deleted)
            match = invert ^ e->deleted;
          break;
        case MUTT_FLAG:
          if (e->flagged != imap_edata_get(e)->flagged)
            match = invert ^ e->flagged;
          break;
        case MUTT_OLD:
          if (e->old != imap_edata_get(e)->old)
            match = invert ^ e->old;
          break;
        case MUTT_READ:
          if (e->read != imap_edata_get(e)->read)
            match = invert ^ e->read;
          break;
        case MUTT_REPLIED:
          if (e->replied != imap_edata_get(e)->replied)
            match = invert ^ e->replied;
          break;
        case MUTT_TAG:
          if (e->tagged)
            match = true;
          break;
        case MUTT_TRASH:
          if (e->deleted && !e->purge)
            match = true;
          break;
        default:
          break;
      }
    }

    if (match && (!changed || e->changed))
    {
      count++;
      if (setstart == 0)
      {
        setstart = imap_edata_get(e)->uid;
        if (started)
        {
          buf_add_printf(buf, ",%u", imap_edata_get(e)->uid);
        }
        else
        {
          buf_add_printf(buf, "%u", imap_edata_get(e)->uid);
          started = true;
        }
      }
      else if (n == (m->msg_count - 1))
      {
        /* tie up if the last message also matches */
        buf_add_printf(buf, ":%u", imap_edata_get(e)->uid);
      }
    }
    else if (setstart)
    {
      /* End current set if message doesn't match. */
      if (imap_edata_get(m->emails[n - 1])->uid > setstart)
        buf_add_printf(buf, ":%u", imap_edata_get(m->emails[n - 1])->uid);
      setstart = 0;
    }
  }

  *pos = n;

  return count;
}

int imap_exec_msgset(struct Mailbox *m, const char *pre, const char *post, enum MessageType flag, bool changed, bool invert)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  struct Email **emails = NULL;
  int pos;
  int rc;
  int count = 0;

  struct Buffer cmd = buf_make(0);

  /* We make a copy of the headers just in case resorting doesn't give
   exactly the original order (duplicate messages?), because other parts of
   the mv are tied to the header order. This may be overkill. */
  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  if (c_sort != SORT_ORDER)
  {
    emails = m->emails;
    if (m->msg_count != 0)
    {
      // We overcommit here, just in case new mail arrives whilst we're sync-ing
      m->emails = mutt_mem_malloc(m->email_max * sizeof(struct Email *));
      memcpy(m->emails, emails, m->email_max * sizeof(struct Email *));

      cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);
      qsort(m->emails, m->msg_count, sizeof(struct Email *), compare_uid);
    }
  }

  pos = 0;

  do
  {
    buf_reset(&cmd);
    buf_add_printf(&cmd, "%s ", pre);
    rc = make_msg_set(m, &cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      buf_add_printf(&cmd, " %s", post);
      if (imap_exec(adata, cmd.data, IMAP_CMD_QUEUE) != IMAP_EXEC_SUCCESS)
      {
        rc = -1;
        goto out;
      }
      count += rc;
    }
  } while (rc > 0);

  rc = count;

out:
  buf_dealloc(&cmd);
  if (c_sort != SORT_ORDER)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
    FREE(&m->emails);
    m->emails = emails;
  }

  return rc;
}

int mbox_add_email(struct Mailbox *m, struct Email *e)
{
  mx_alloc_memory(m);
  m->emails[m->msg_count] = e;
  m->msg_count++;
  return 0;
}

struct ImapEmailData *imap_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ImapEmailData));
}

void imap_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapEmailData *edata = *ptr;
  /* this should be safe even if the list wasn't used */
  FREE(&edata->flags_system);
  FREE(&edata->flags_remote);
  FREE(ptr);
}

void imap_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

struct ImapAccountData *imap_adata_new(struct Account *a)
{
  struct ImapAccountData *adata = mutt_mem_calloc(1, sizeof(struct ImapAccountData));
  adata->account = a;

  static unsigned char new_seqid = 'a';

  adata->seqid = new_seqid;

  if (++new_seqid > 'z')
    new_seqid = 'a';

  return adata;
}

int main()
{
  const int num_emails = 1000;

  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Sort);
  cs_register_variables(cs, MainVars, DT_NO_FLAGS);

  NeoMutt = neomutt_new(cs);

  struct Mailbox *m = mailbox_new();
  struct Account *a = account_new(NULL, NeoMutt->sub);
  m->account = a;
  m->type = MUTT_IMAP;
  struct ImapAccountData *adata = imap_adata_new(a);
  adata->mailbox = m;
  a->adata = adata;
  a->adata_free = imap_adata_free;

  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = email_new();
    struct ImapEmailData *edata = imap_edata_new();
    edata->uid = 1000 + i;
    e->active = true;
    e->lines = rand() % 65536;
    if ((rand() % 100) > 30)
      e->flagged = true;
    e->edata = edata;
    e->edata_free = imap_edata_free;
    mbox_add_email(m, e);
  }

  qsort(m->emails, m->msg_count, sizeof(struct Email *), compare_lines);

  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    struct ImapEmailData *edata = e->edata;
    if (e->flagged)
      printf("\033[1;32m");
    else
      printf("\033[31m");

    printf("%u ", edata->uid);
    printf("\033[0m");
  }
  printf("\n\n");

  int count = 0;
  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    if (e->flagged)
      count++;
  }
  printf("%d flagged emails\n", count);
  printf("\n");

  enum MessageType flag = MUTT_FLAG;
  bool changed = false;
  bool invert = false;
  int rc = imap_exec_msgset(m, "PRE", "POST", flag, changed, invert);

  printf("rc = %d\n", rc);

  mailbox_free(&m);
  account_free(&a);

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  return 0;
}
