#include "search.h"
#include "../app/app.h" // IWYU pragma: keep

/* internal helpers */
static int str_icontains(const char *hay, const char *needle);
static void search_add_result(const char *name, const char *path);
static void search_walk(const char *dir, int depth);
static void run_search(void);

/* Search (X button): on-screen keyboard → filtered results.
 * Scope = the folder you were in (ROMS_PATH root = search everything). */
bool search_kbd_active = false; /* typing the query */
bool viewing_search = false;    /* showing results list */
char search_query[64] = "";
int search_kbd_r = 0, search_kbd_c = 0;
char search_scope[MAX_PATH_LEN] = "";
static SearchResult *search_results = NULL;
static int search_results_count = 0, search_results_cap = 0;
/* On-screen keyboard layout. Rows 0-3 are character keys; row 4 is special. */
const char *KBD_ROWS[4] = {
    "1234567890",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
};
const char *KBD_SPECIAL[3] = {"SPACE", "DEL", "GO"};
int kbd_row_len(int r) {
  return (r == KBD_SPECIAL_ROW) ? 3 : (int)strlen(KBD_ROWS[r]);
}
static int str_icontains(const char *hay, const char *needle) {
  if (!needle[0])
    return 1;
  for (; *hay; hay++) {
    const char *h = hay, *n = needle;
    while (*h && *n &&
           tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      h++;
      n++;
    }
    if (!*n)
      return 1;
  }
  return 0;
}
static void search_add_result(const char *name, const char *path) {
  if (search_results_count >= search_results_cap) {
    int nc = search_results_cap ? search_results_cap * 2 : 128;
    SearchResult *nr = realloc(search_results, nc * sizeof(SearchResult));
    if (!nr)
      return;
    search_results = nr;
    search_results_cap = nc;
  }
  SearchResult *r = &search_results[search_results_count];
  strncpy(r->name, name, sizeof(r->name) - 1);
  r->name[sizeof(r->name) - 1] = '\0';
  strncpy(r->path, path, sizeof(r->path) - 1);
  r->path[sizeof(r->path) - 1] = '\0';
  search_results_count++;
}
static void search_walk(const char *dir, int depth) {
  DIR *d = opendir(dir);
  if (!d)
    return;
  struct dirent *e;
  char sys_folder[64];
  const char *sf = get_console_folder(dir);
  if (sf) {
    strncpy(sys_folder, sf, sizeof(sys_folder) - 1);
    sys_folder[sizeof(sys_folder) - 1] = '\0';
  } else
    sys_folder[0] = '\0';
  while ((e = readdir(d)) && search_results_count < 2000) {
    if (e->d_name[0] == '.')
      continue;
    char p[MAX_PATH_LEN];
    snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(p, &st) != 0)
      continue;
    if (S_ISDIR(st.st_mode)) {
      if (!is_psp_folder(e->d_name) && depth < 3)
        search_walk(p, depth + 1);
    } else if (str_icontains(e->d_name, search_query)) {
      /* Same per-system whitelist as the browser, so search results never
       * resurrect the hidden companion files (PS1 .bin tracks). The
       * extension is isolated once here (review: normalize per entry). */
      const char *ext = strrchr(e->d_name, '.');
      if (ext_filter_should_hide(sys_folder[0] ? sys_folder : NULL,
                                 ext ? ext + 1 : ""))
        continue;
      search_add_result(e->d_name, p);
    }
  }
  closedir(d);
}
static void run_search(void) {
  search_results_count = 0;
  if (search_query[0])
    search_walk(search_scope[0] ? search_scope : ROMS_PATH, 0);

  /* mirror results into entries[] for the shared list renderer */
  while (search_results_count > entry_capacity) {
    entry_capacity =
        entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
    entries = realloc(entries, entry_capacity * sizeof(DirEntry));
    if (!entries)
      return;
  }
  entry_count = 0;
  for (int i = 0; i < search_results_count; i++) {
    strncpy(entries[entry_count].name, search_results[i].name, 255);
    entries[entry_count].name[255] = '\0';
    entries[entry_count].is_dir = 0;
    entry_count++;
  }
  viewing_search = true;
  search_kbd_active = false;
  selected_index = 0;
  scroll_offset = 0;
}
void search_launch(int idx) {
  if (idx < 0 || idx >= search_results_count)
    return;
  launch_by_path(search_results[idx].path);
}
void handle_search_kbd(void) {
  if (input_was_pressed(FROG_BTN_UP))
    search_kbd_r = (search_kbd_r - 1 + KBD_NROWS) % KBD_NROWS;
  if (input_was_pressed(FROG_BTN_DOWN))
    search_kbd_r = (search_kbd_r + 1) % KBD_NROWS;
  if (input_was_pressed(FROG_BTN_LEFT))
    search_kbd_c--;
  if (input_was_pressed(FROG_BTN_RIGHT))
    search_kbd_c++;
  {
    int rl = kbd_row_len(search_kbd_r);
    if (search_kbd_c < 0)
      search_kbd_c = rl - 1;
    if (search_kbd_c >= rl)
      search_kbd_c = 0;
  }
  if (input_was_pressed(FROG_BTN_A)) {
    int len = (int)strlen(search_query);
    if (search_kbd_r == KBD_SPECIAL_ROW) {
      if (search_kbd_c == 0) {
        if (len < (int)sizeof(search_query) - 1) {
          search_query[len] = ' ';
          search_query[len + 1] = '\0';
        }
      } else if (search_kbd_c == 1) {
        if (len > 0)
          search_query[len - 1] = '\0';
      } else
        run_search();
    } else if (len < (int)sizeof(search_query) - 1) {
      search_query[len] = KBD_ROWS[search_kbd_r][search_kbd_c];
      search_query[len + 1] = '\0';
    }
  }
  if (input_was_pressed(FROG_BTN_Y)) { /* quick backspace */
    int len = (int)strlen(search_query);
    if (len > 0)
      search_query[len - 1] = '\0';
  }
  if (input_was_pressed(FROG_BTN_START))
    run_search();
  if (input_was_pressed(FROG_BTN_B)) { /* cancel → restore browser list */
    search_kbd_active = false;
    scan_directory(current_path);
    selected_index = 0;
    scroll_offset = 0;
  }
}
void render_search_kbd(void) {
  render_clear_screen(framebuffer);
  render_header(framebuffer, tr("header.search"));

  int y = START_Y;
  char q[96];
  snprintf(q, sizeof(q), "> %s_", search_query);
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y, q,
                 COLOR_TEXT);
  y += ITEM_HEIGHT + UI_S(8);

  int cw = UI_S(26), ch = ITEM_HEIGHT;
  for (int r = 0; r < KBD_NROWS; r++) {
    int rl = kbd_row_len(r);
    int ry = y + r * ch;
    for (int c = 0; c < rl; c++) {
      char lbl[8];
      int cx;
      if (r == KBD_SPECIAL_ROW) {
        snprintf(lbl, sizeof(lbl), "%s", KBD_SPECIAL[c]);
        cx = PADDING + c * (cw * 3);
      } else {
        lbl[0] = KBD_ROWS[r][c];
        lbl[1] = '\0';
        cx = PADDING + c * cw;
      }
      if (r == search_kbd_r && c == search_kbd_c)
        render_text_pillbox(framebuffer, cx, ry, lbl, COLOR_SELECT_BG,
                            COLOR_SELECT_TEXT, 7, 0);
      else
        font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, cx, ry, lbl,
                       COLOR_TEXT);
    }
  }
  render_legend(framebuffer, LEGEND_X_NONE, 0, 0);
}
