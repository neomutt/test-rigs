#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "menu/lib.h"
#include "pattern/lib.h"

char *HomeDir;
keycode_t AbortKey;
bool OptForceRefresh;
bool OptIgnoreMacroEvents;
bool OptKeepQuiet;
bool OptNoCurses;
int SigInt;
int SigWinch;

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "arrow_cursor", DT_BOOL, false, 0, NULL, NULL, 0 },
  { "arrow_string", DT_STRING|DT_NOT_EMPTY, IP "->", 0, NULL, NULL, 0 },
  { "braille_friendly", DT_BOOL, false, 0, NULL, NULL, 0 },
  { "menu_context", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL, NULL, 0 },
  { "menu_move_off", DT_BOOL, true, 0, NULL, NULL, 0 },
  { "menu_scroll", DT_BOOL, false, 0, NULL, NULL, 0 },
  { NULL },
  // clang-format on
};

bool attr_color_is_set(struct AttrColor *ac)
{
  return false;
}

int buf_get_field(const char *field, struct Buffer *buf, CompletionFlags complete,
                  bool multiple, struct Mailbox *m, char ***files, int *numfiles)
{
  return 0;
}

void buf_select_file(struct Buffer *file, SelectFileFlags flags,
                     struct Mailbox *m, char ***files, int *numfiles)
{
}

struct Mailbox *get_current_mailbox(void)
{
  return NULL;
}

struct MuttWindow *helpbar_new(void)
{
  return NULL;
}

struct AttrColor *merged_color_overlay(struct AttrColor *base, struct AttrColor *over)
{
  return NULL;
}

void mutt_clear_error(void)
{
}

struct Email *mutt_get_virt_email(struct Mailbox *m, int vnum)
{
  return NULL;
}

int mutt_monitor_poll(void)
{
  return 0;
}

bool mutt_pattern_exec(struct Pattern *pat, PatternExecFlags flags,
                       struct Mailbox *m, struct Email *e, struct PatternCache *cache)
{
  return false;
}

void mutt_resize_screen(void)
{
}

int mutt_system(const char *cmd)
{
  return 0;
}

enum QuadOption mutt_yesorno(const char *msg, enum QuadOption def)
{
  return MUTT_NO;
}

struct RegexColorList *regex_colors_get_list(enum ColorId cid)
{
  return false;
}

struct Menu *menu_new(enum MenuType type, struct MuttWindow *win, struct ConfigSubset *sub);
void menu_free(struct Menu **ptr);

struct AttrColor *simple_color_get(enum ColorId cid)
{
  return NULL;
}

void mutt_color_observer_remove(observer_t callback, void *global_data)
{
}

void mutt_color_observer_add(observer_t callback, void *global_data)
{
}

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

  NeoMutt = neomutt_new(cs);

  cs_register_variables(cs, MainVars, DT_NO_FLAGS);
  return NeoMutt;
}

void test_neomutt_destroy(void)
{
  struct ConfigSet *cs = NeoMutt->sub->cs;
  neomutt_free(&NeoMutt);
  cs_free(&cs);
}

struct Private
{
  int num;
};

void window_recalc(struct MuttWindow *win)
{
  if (win->recalc)
    win->recalc(win);
  win->actions &= ~WA_RECALC;
}

void window_repaint(struct MuttWindow *win)
{
  if (win->repaint)
    win->repaint(win);
  win->actions &= ~WA_REPAINT;
}

void test_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  snprintf(buf, buflen, "This is line: %d\n", line);
}

static void menu_wdata_free(struct MuttWindow *win, void **ptr)
{
  menu_free((struct Menu **) ptr);
}

int menu_repaint(struct MuttWindow *win)
{
  struct Menu *menu = win->wdata;
  menu->redraw |= MENU_REDRAW_FULL;
  menu_redraw(menu);
  menu->redraw = MENU_REDRAW_NO_FLAGS;

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  const bool c_braille_friendly = cs_subset_bool(menu->sub, "braille_friendly");

  /* move the cursor out of the way */
  if (c_arrow_cursor)
    mutt_window_move(menu->win, 2, menu->current - menu->top);
  else if (c_braille_friendly)
    mutt_window_move(menu->win, 0, menu->current - menu->top);
  else
  {
    mutt_window_move(menu->win, menu->win->state.cols - 1, menu->current - menu->top);
  }

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

struct MuttWindow *menu_window_new(enum MenuType type, struct ConfigSubset *sub)
{
  struct MuttWindow *win = mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);

  struct Menu *menu = menu_new(type, win, sub);

  win->repaint = menu_repaint;
  win->wdata = menu;
  win->wdata_free = menu_wdata_free;
  win->actions |= WA_RECALC;

  return win;
}

// int main(int argc, char *argv[])
int main()
{
  // if (argc != 2)
  //   return 1;

  NeoMutt = test_neomutt_create();
  if (!NeoMutt)
    return 1;

  struct MuttWindow *win = menu_window_new(MENU_INDEX, NeoMutt->sub);

  struct Private priv = { 42 };

  struct Menu *menu = win->wdata;
  menu->mdata = &priv;
  menu->make_entry = test_make_entry;
  menu->max = 10;

  window_recalc(win);
  window_repaint(win);

  mutt_window_free(&win);

  test_neomutt_destroy();
  return 0;
}
