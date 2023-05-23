#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "imap/adata.h"
#include "imap/edata.h"
#include "imap/private.h"
#include "limits.h"
#include "mx.h"
#include "sort.h"

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "sort", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT, SORT_DATE, 0, NULL, },
  { "imap_pipeline_depth", DT_NUMBER|DT_NOT_NEGATIVE, 15, 0, NULL, },
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

int compare_lines(const void *a, const void *b)
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
  int i;
  bool started = false;

  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  for (i = *pos; (i < m->msg_count) && (buf_len(buf) < IMAP_MAX_CMDLEN); i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    struct ImapEmailData *edata = imap_edata_get(e);

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
          if (e->deleted != edata->deleted)
            match = invert ^ e->deleted;
          break;
        case MUTT_FLAG:
          if (e->flagged != edata->flagged)
            match = invert ^ e->flagged;
          break;
        case MUTT_OLD:
          if (e->old != edata->old)
            match = invert ^ e->old;
          break;
        case MUTT_READ:
          if (e->read != edata->read)
            match = invert ^ e->read;
          break;
        case MUTT_REPLIED:
          if (e->replied != edata->replied)
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
        setstart = edata->uid;
        if (started)
        {
          buf_add_printf(buf, ",%u", edata->uid);
        }
        else
        {
          buf_add_printf(buf, "%u", edata->uid);
          started = true;
        }
      }
      else if (i == (m->msg_count - 1))
      {
        /* tie up if the last message also matches */
        buf_add_printf(buf, ":%u", edata->uid);
      }
    }
    else if (setstart != 0)
    {
      e = m->emails[i - 1];
      edata = imap_edata_get(e);
      const unsigned int uid = edata->uid;

      /* End current set if message doesn't match. */
      if (uid > setstart)
        buf_add_printf(buf, ":%u", uid);
      setstart = 0;
    }
  }

  *pos = i;

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

int main(int argc, char *argv[])
{
  time_t t = 0;

  if (argc == 2)
  {
    unsigned long num = 0;
    mutt_str_atoul(argv[1], &num);
    t = num;
  }
  else
  {
    t = time(NULL);
  }

  printf("seed: %ld\n", t);
  srand(t);
  const int num_emails = 100;

  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, Number);
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
      e->tagged = true;
    e->edata = edata;
    e->edata_free = imap_edata_free;
    mbox_add_email(m, e);
  }

  // qsort(m->emails, m->msg_count, sizeof(struct Email *), compare_lines);
  qsort(m->emails, m->msg_count, sizeof(struct Email *), compare_uid);

  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    struct ImapEmailData *edata = e->edata;
    if (e->tagged)
      printf("\033[1;32m");
    else
      printf("\033[1;31m");

    printf("%u ", edata->uid);
    printf("\033[0m");
  }
  printf("\n\n");

  int count = 0;
  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    if (e->tagged)
      count++;
  }
  printf("%d tagged emails\n", count);
  printf("\n");

  enum MessageType flag = MUTT_TAG;
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
