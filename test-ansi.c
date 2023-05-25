#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "color/lib.h"
#include "gui/lib.h"

struct Mailbox;
struct Menu;
struct PatternList;

struct NeoMutt *NeoMutt = NULL;
bool OptNoCurses = false;
char *HomeDir;

typedef uint16_t TokenFlags;
typedef uint8_t PatternCompFlags;

extern struct AttrColorList MergedColors;
extern struct CursesColorList CursesColors;

struct MailboxView *get_current_mailbox_view(void)
{
  return NULL;
}

struct Mailbox *get_current_mailbox(void)
{
  return NULL;
}

struct Menu *get_current_menu(void)
{
  return NULL;
}

struct PatternList *mutt_pattern_comp(struct Mailbox *m, struct Menu *menu, const char *s,
                                      PatternCompFlags flags, struct Buffer *err)
{
  return NULL;
}

void mutt_check_simple(struct Buffer *buf, const char *simple)
{
}

int parse_extract_token(struct Buffer *dest, struct Buffer *tok, TokenFlags flags)
{
  return 0;
}

void mutt_pattern_free(struct PatternList **pat)
{
}

int main(int argc, char *argv[])
{
  if (argc != 2)
    return 1;

  initscr();
  start_color();
  use_default_colors();
  endwin();
  printf("%d colours\n", COLORS);

  TAILQ_INIT(&MergedColors);
  TAILQ_INIT(&CursesColors);

  struct AnsiColor ansi = { 0 };
  int rc = ansi_color_parse(argv[1], &ansi, &MergedColors, false);

  printf("%d\n", rc);

  return 0;
}
