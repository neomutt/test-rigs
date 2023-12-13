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
bool StartupComplete = false;

bool config_init_hcache(struct ConfigSet *cs);
bool config_init_maildir(struct ConfigSet *cs);

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "autocrypt", DT_BOOL, false, 0, NULL, },
  { "assumed_charset", DT_SLIST|SLIST_SEP_COLON|SLIST_ALLOW_EMPTY, 0, 0, charset_slist_validator, },
  { "auto_subscribe", DT_BOOL, false, 0, NULL, },
  { "charset", DT_STRING|DT_NOT_EMPTY|DT_CHARSET_SINGLE, 0, 0, charset_validator, },
  { "header_cache", DT_PATH, 0, 0, NULL, },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, NULL, },
  { "rfc2047_parameters", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

const struct ComprOps *compress_get_ops(const char *compr)
{
  return NULL;
}

int mutt_autocrypt_process_autocrypt_header(struct Email *e, struct Envelope *env)
{
  return 0;
}

void mutt_encode_path(struct Buffer *buf, const char *src)
{
  char *p = mutt_str_dup(src);
  int rc = mutt_ch_convert_string(&p, cc_charset(), "us-ascii", MUTT_ICONV_NO_FLAGS);
  size_t len = buf_strcpy(buf, (rc == 0) ? NONULL(p) : NONULL(src));

  /* convert the path to POSIX "Portable Filename Character Set" */
  for (size_t i = 0; i < len; i++)
  {
    if (!isalnum(buf->data[i]) && !strchr("/.-_", buf->data[i]))
    {
      buf->data[i] = '_';
    }
  }
  FREE(&p);
}

void nm_edata_free(void **ptr)
{
}

int nm_update_filename(struct Mailbox *m, const char *old_file,
                       const char *new_file, struct Email *e)
{
  return 0;
}

void progress_set_message(struct Progress *progress, const char *fmt, ...)
{
}

void progress_set_size(struct Progress *progress, size_t size)
{
}

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

  NeoMutt = neomutt_new(cs);

  cs_register_variables(cs, MainVars, DT_NO_FLAGS);
  config_init_maildir(cs);
  config_init_hcache(cs);
  return NeoMutt;
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

void mx_alloc_memory(struct Mailbox *m, int req_size)
{
  if ((req_size + 1) <= m->email_max)
    return;

  // Step size to increase by
  // Larger mailboxes get a larger step (limited to 1000)
  const int grow = CLAMP(m->email_max, 25, 1000);

  // Sanity checks
  req_size = ROUND_UP(req_size + 1, grow);

  const size_t s = MAX(sizeof(struct Email *), sizeof(int));
  if ((req_size * s) < (m->email_max * s))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  if (m->emails)
  {
    mutt_mem_realloc(&m->emails, req_size * sizeof(struct Email *));
    mutt_mem_realloc(&m->v2r, req_size * sizeof(int));
  }
  else
  {
    m->emails = mutt_mem_calloc(req_size, sizeof(struct Email *));
    m->v2r = mutt_mem_calloc(req_size, sizeof(int));
  }

  for (int i = m->email_max; i < req_size; i++)
  {
    m->emails[i] = NULL;
    m->v2r[i] = -1;
  }

  m->email_max = req_size;
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

  if (!test_neomutt_create())
    return 1;

  int rc = 1;
  cs_str_string_set(NeoMutt->sub->cs, "header_cache", "p/cache", NULL);
  cs_str_string_set(NeoMutt->sub->cs, "header_cache_backend", "lmdb", NULL);

  const struct MxOps *ops = &MxMaildirOps;

  struct Mailbox *m = mailbox_new();
  m->type = MUTT_MAILDIR;
  m->verbose = true;
  buf_strcpy(&m->pathbuf, argv[1]);

  enum MxOpenReturns rc_m = ops->mbox_open(m);
  if (rc_m != MX_OPEN_OK)
  {
    printf("open failed for %s\n", mailbox_path(m));
    goto done;
  }

  printf("%d emails in %s\n", m->msg_count, mailbox_path(m));
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    printf("\t%s\n", e->path);
    if (i > 8)
    {
      printf("\t...\n");
      break;
    }
  }

  rc = 0;

done:
  ops->mbox_close(m);
  mailbox_free(&m);

  struct ConfigSet *cs = NeoMutt->sub->cs;
  neomutt_free(&NeoMutt);
  cs_free(&cs);

  return rc;
}
