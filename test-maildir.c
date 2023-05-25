#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "maildir/lib.h"
#include "copy.h"
#include "mx.h"

struct Progress;

char *HomeDir;
char *ShortHostname;
bool MonitorContextChanged;
int SigInt;

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "assumed_charset", DT_SLIST|SLIST_SEP_COLON|SLIST_ALLOW_EMPTY, 0, 0, charset_slist_validator, },
  { "auto_subscribe", DT_BOOL, false, 0, NULL, },
  { "charset", DT_STRING|DT_NOT_EMPTY|DT_CHARSET_SINGLE, 0, 0, charset_validator, },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, NULL, },
  { "rfc2047_parameters", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

struct NeoMutt *test_neomutt_create(void)
{
  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Address);
  CONFIG_INIT_TYPE(cs, Bool);
  CONFIG_INIT_TYPE(cs, Enum);
  CONFIG_INIT_TYPE(cs, Long);
  CONFIG_INIT_TYPE(cs, Mbtable);
  CONFIG_INIT_TYPE(cs, Number);
  CONFIG_INIT_TYPE(cs, Path);
  CONFIG_INIT_TYPE(cs, Quad);
  CONFIG_INIT_TYPE(cs, Regex);
  CONFIG_INIT_TYPE(cs, Slist);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, String);

  struct NeoMutt *n = neomutt_new(cs);

  cs_register_variables(cs, MainVars, DT_NO_FLAGS);
  return n;
}

int mutt_copy_message(FILE *fp_out, struct Email *e, struct Message *msg, CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen)
{
  return 0;
}

void mutt_set_flag_update(struct Mailbox *m, struct Email *e, enum MessageType flag, bool bf, bool upd_mbox)
{
}

void mutt_set_flag(struct Mailbox *m, struct Email *e, enum MessageType flag, bool bf, bool upd_mbox)
{
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

int mx_msg_close(struct Mailbox *m, struct Message **msg)
{
  return 0;
}

struct Message *mx_msg_open(struct Mailbox *m, struct Email *e)
{
  return NULL;
}

struct Message *mx_msg_open_new(struct Mailbox *m, const struct Email *e, MsgOpenFlags flags)
{
  return NULL;
}

enum ProgressType
{
  MUTT_PROGRESS_READ,
  MUTT_PROGRESS_WRITE,
  MUTT_PROGRESS_NET
};

void progress_free(struct Progress **ptr)
{
}

struct Progress *progress_new(const char *msg, enum ProgressType type, size_t size)
{
  return NULL;
}

bool progress_update(struct Progress *progress, size_t pos, int percent)
{
  return false;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
    return 1;

  NeoMutt = test_neomutt_create();
  if (!NeoMutt)
    return 1;

  const struct MxOps *ops = &MxMaildirOps;

  struct Mailbox m = { 0 };
  m.type = MUTT_MAILDIR;
  buf_strcpy(&m.pathbuf, argv[1]);

  enum MxOpenReturns rc = ops->mbox_open(&m);
  if (rc != MX_OPEN_OK)
  {
    printf("open failed\n");
    return 1;
  }

  printf("%d emails in %s\n", m.msg_count, buf_string(&m.pathbuf));
  for (int i = 0; i < m.msg_count; i++)
  {
    struct Email *e = m.emails[i];
    printf("\t%s\n", e->path);
    if (i > 8)
    {
      printf("\t...\n");
      break;
    }
  }

  return 0;
}
