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
#include "imap/msg_set.h"
#include "limits.h"
#include "mx.h"
#include "sort.h"

extern int ImapMaxCmdlen;

const int num_emails = 5000;
const int num_tests = 100;
struct Buffer buf_old = { 0 };
struct Buffer buf_new = { 0 };
int rc_old = 0;
int rc_new = 0;
int success = 0;

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "sort", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT, SORT_DATE, 0, NULL, NULL, 0 },
  { "imap_pipeline_depth", DT_NUMBER|DT_NOT_NEGATIVE, 15, 0, NULL, NULL, 0 },
  { NULL },
};

int imap_exec(struct ImapAccountData *adata, const char *cmdstr, ImapCmdFlags flags)
{
  return 0;
}

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
    mutt_mem_realloc(&m->v2r, m->email_max * sizeof(int));
  }
  else
  {
    m->emails = mutt_mem_calloc(m->email_max, sizeof(struct Email *));
    m->v2r = mutt_mem_calloc(m->email_max, sizeof(int));
  }
  for (int i = m->email_max - grow; i < m->email_max; i++)
  {
    if (i < m->email_max)
    {
      m->emails[i] = NULL;
      m->v2r[i] = -1;
    }
  }
}

void nm_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;
}

int compare_lines(const void *a, const void *b)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  return mutt_numeric_cmp(ea->lines, eb->lines);
}

static int imap_sort_email_uid(const void *a, const void *b)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  const unsigned int ua = imap_edata_get((struct Email *) ea)->uid;
  const unsigned int ub = imap_edata_get((struct Email *) eb)->uid;

  return mutt_numeric_cmp(ua, ub);
}

int imap_exec1(struct ImapAccountData *adata, const char *cmdstr, ImapCmdFlags flags)
{
  if (!adata || (flags == IMAP_CMD_NO_FLAGS))
    return -1;

  buf_add_printf(&buf_old, "%s [%lu]\n", cmdstr, strlen(cmdstr));
  // printf("%s\n", cmdstr);
  return 0;
}

int imap_make_msg_set1(struct Mailbox *m, struct Buffer *buf, enum MessageType flag, bool changed, bool invert, int *pos)
{
  int count = 0;             /* number of messages in message set */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;

  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  for (n = *pos; (n < m->msg_count) && (buf_len(buf) < ImapMaxCmdlen); n++)
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

int imap_exec_msgset1(struct Mailbox *m, const char *pre, const char *post, enum MessageType flag, bool changed, bool invert)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  int pos;
  int rc;
  int count = 0;

  struct Buffer cmd = buf_make(ImapMaxCmdlen);

  pos = 0;

  do
  {
    buf_reset(&cmd);
    buf_add_printf(&cmd, "%s ", pre);
    rc = imap_make_msg_set1(m, &cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      buf_add_printf(&cmd, " %s", post);
      if (imap_exec1(adata, cmd.data, IMAP_CMD_QUEUE) != IMAP_EXEC_SUCCESS)
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
  return rc;
}

int imap_exec2(struct ImapAccountData *adata, const char *cmdstr, ImapCmdFlags flags)
{
  if (!adata || (flags == IMAP_CMD_NO_FLAGS))
    return -1;

  buf_add_printf(&buf_new, "%s [%lu]\n", cmdstr, strlen(cmdstr));
  // printf("%s\n", cmdstr);
  return 0;
}

int imap_make_msg_set2(struct UidArray *uida, struct Buffer *buf, int *pos)
{
  if (!uida || !buf || !pos)
    return 0;

  const size_t array_size = ARRAY_SIZE(uida);
  if ((array_size == 0) || (*pos >= array_size))
    return 0;

  int count = 1; // Number of UIDs added to the set
  size_t i = *pos;
  unsigned int start = *ARRAY_GET(uida, i);
  unsigned int prev = start;

  for (i++; (i < array_size) && (buf_len(buf) < ImapMaxCmdlen); i++, count++)
  {
    unsigned int uid = *ARRAY_GET(uida, i);

    // Keep adding to current set
    if (uid == (prev + 1))
    {
      prev = uid;
      continue;
    }

    // End the current set
    if (start == prev)
      buf_add_printf(buf, "%u,", start);
    else
      buf_add_printf(buf, "%u:%u,", start, prev);

    // Start a new set
    start = uid;
    prev = uid;
  }

  if (start == prev)
    buf_add_printf(buf, "%u", start);
  else
    buf_add_printf(buf, "%u:%u", start, prev);

  *pos = i;

  return count;
}

int imap_exec_msgset2(struct Mailbox *m, const char *pre, const char *post, struct UidArray *uida)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  int pos = 0;
  int rc = 0;
  int count = 0;

  struct Buffer cmd = buf_make(ImapMaxCmdlen);

  do
  {
    buf_printf(&cmd, "%s ", pre);
    rc = imap_make_msg_set2(uida, &cmd, &pos);
    if (rc > 0)
    {
      buf_add_printf(&cmd, " %s", post);
      if (imap_exec2(adata, cmd.data, IMAP_CMD_QUEUE) != IMAP_EXEC_SUCCESS)
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
  return rc;
}

int mbox_add_email(struct Mailbox *m, struct Email *e)
{
  mx_alloc_memory(m);
  m->emails[m->msg_count] = e;
  m->msg_count++;
  return 0;
}

static int select_email_uids(struct Email **emails, int num_emails, enum MessageType flag, bool changed, bool invert, struct UidArray *uida)
{
  if (!emails || !uida)
    return -1;

  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = emails[i];

    /* don't include pending expunged messages.
     *
     * TODO: can we unset active in cmd_parse_expunge() and
     * cmd_parse_vanished() instead of checking for index != INT_MAX. */
    if (!e || !e->active || (e->index == INT_MAX))
      continue;

    struct ImapEmailData *edata = imap_edata_get(e);

    if (e->tagged && (!changed || e->changed))
      ARRAY_ADD(uida, edata->uid);
  }

  return ARRAY_SIZE(uida);
}

void dump_emails(struct Mailbox *m, int num_emails)
{
  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    struct ImapEmailData *edata = e->edata;
    if (e->tagged)
      printf("\033[1;32m");
    // else
    //   printf("\033[1;31m");

    if (e->tagged)
      printf("%u ", edata->uid);
    else
      printf(". ");
    printf("\033[0m");
  }
  printf("\n\n");
}

void test_msgset()
{
  buf_reset(&buf_old);
  buf_reset(&buf_new);
  rc_old = 0;
  rc_new = 0;

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
  qsort(m->emails, m->msg_count, sizeof(struct Email *), imap_sort_email_uid);

  int count = 0;
  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = m->emails[i];
    if (e->tagged)
      count++;
  }
  // printf("%d tagged emails\n\n", count);

  enum MessageType flag = MUTT_TAG;
  bool changed = false;
  bool invert = false;

  struct UidArray uida = ARRAY_HEAD_INITIALIZER;
  select_email_uids(m->emails, m->msg_count, flag, changed, invert, &uida);

  rc_old = imap_exec_msgset1(m, "PRE", "POST", flag, changed, invert);
  // printf("rc_old = %d\n\n", rc_old);

  rc_new = imap_exec_msgset2(m, "PRE", "POST", &uida);
  // printf("rc_new = %d\n\n", rc_new);

  if ((rc_old == rc_new) && (strcmp(buf_string(&buf_old), buf_string(&buf_new)) == 0))
  {
    // printf("rc = %d\n", rc_old);
    success++;
  }
  else
  {
    dump_emails(m, num_emails);
    printf("%s", buf_string(&buf_old));
    printf("rc_old = %d\n", rc_old);
    printf("\n");
    printf("%s", buf_string(&buf_new));
    printf("rc_new = %d\n", rc_new);
    printf("\n");
  }

  ARRAY_FREE(&uida);

  mailbox_free(&m);
  account_free(&a);
}

int main(int argc, char *argv[])
{
  // ImapMaxCmdlen = 50;

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

  srand(t);

  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, Number);
  cs_register_variables(cs, MainVars, DT_NO_FLAGS);

  NeoMutt = neomutt_new(cs);

  buf_alloc(&buf_old, 2048);
  buf_alloc(&buf_new, 2048);

  for (int i = 0; i < num_tests; i++)
    test_msgset();

  buf_dealloc(&buf_old);
  buf_dealloc(&buf_new);

  int col = (num_tests == success) ? 32 : 31;
  printf("seed: %ld, emails: %d, tests %d, \033[1;%dmerrors %d\033[0m\n", t, num_emails, num_tests, col, num_tests - success);

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  return 0;
}
