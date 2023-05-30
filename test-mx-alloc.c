#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"

void nm_edata_free(void **ptr)
{
}

void mx_alloc_memory(struct Mailbox *m, int req_size)
{
  const int grow = 25;

  // Sanity checks
  req_size = MAX(req_size, m->email_max);
  req_size = ROUND_UP(req_size, grow);

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

void dump_emails(struct Mailbox *m)
{
  printf("M:");
  for (int i = 0; i < m->email_max; i++)
  {
    printf("%c", m->emails[i] ? 'E' : '.');
  }
  printf("\n");
}

int main(int argc, char *argv[])
{
  if (argc != 3)
    return 1;

  int size1 = 0;
  mutt_str_atoi(argv[1], &size1);
  if (size1 < 0)
    return 1;

  int size2 = 0;
  mutt_str_atoi(argv[2], &size2);
  if (size2 < 0)
    return 1;

  struct Mailbox *m = mailbox_new();
  m->email_max = 0;
  FREE(&m->emails);
  FREE(&m->v2r);

  mx_alloc_memory(m, size1);

  for (int i = 0; i < size1; i++)
  {
    m->emails[m->msg_count++] = email_new();
  }

  dump_emails(m);

  printf("%p (%d)\n", m->emails, m->email_max);
  printf("%p (%d)\n", m->v2r, m->vcount);

  mx_alloc_memory(m, size2);

  dump_emails(m);

  printf("%p (%d)\n", m->emails, m->email_max);
  printf("%p (%d)\n", m->v2r, m->vcount);

  mailbox_free(&m);
  return 0;
}
