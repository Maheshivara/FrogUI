#include "../app/app.h"

/* Greedy word wrap into at most `max_lines` lines of width `w` px. The last
 * line gets ".." when text remains. Returns the y below the last line drawn. */
static int draw_wrapped(uint16_t *fb, int x, int y, int w, int lh,
                        int max_lines, const char *text, uint16_t color) {
  const char *p = text;
  int lines = 0;
  while (*p && lines < max_lines) {
    while (*p == ' ' || *p == '\r')
      p++;
    if (!*p)
      break;
    char buf[200];
    int n = 0, brk = -1;
    while (p[n] && p[n] != '\n' && n < (int)sizeof buf - 1) {
      buf[n] = p[n];
      buf[n + 1] = '\0';
      if (p[n] == ' ')
        brk = n;
      if (font_measure_text(buf) > w) {
        if (brk > 0)
          n = brk;
        break;
      }
      n++;
    }
    if (n == 0)
      n = 1;
    buf[n] = '\0';
    if (lines == max_lines - 1 && p[n] && n >= 3) {
      buf[n - 2] = '.';
      buf[n - 1] = '.';
    }
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, buf, color);
    y += lh;
    lines++;
    p += n;
    if (*p == '\n')
      p++;
  }
  return y;
}

/* Opaque-free repaint: the real background (banner slice or theme bg) is
 * redrawn first, so list-row overflow on the left is painted over and
 * transparent art shows the true background. */
void render_boxart_panel(uint16_t *fb, const char *full_path,
                         const char *name) {
  int px = SCREEN_WIDTH * 58 / 100;
  int top = UI_S(44);
  int bot = SCREEN_HEIGHT - UI_S(40);
  int pw = SCREEN_WIDTH - px - UI_S(10);
  int ph = bot - top;
  if (pw < UI_S(60) || ph < UI_S(60))
    return; /* too narrow to bother */

  /* Cache the decoded thumbnail; only re-decode when the selection changes. */
  static char cached_path[1024] = "";
  static Thumbnail ctb;
  static int chas = 0;
  if (strcmp(full_path, cached_path) != 0) {
    if (chas) {
      free_thumbnail(&ctb);
      chas = 0;
    }
    if (game_art_load(full_path, ARTWORK_BOXART, &ctb))
      chas = 1;
    /* Scrapers often provide a title screen rather than box art. */
    if (!chas && game_art_load(full_path, ARTWORK_TITLE_SCREEN, &ctb))
      chas = 1;
    strncpy(cached_path, full_path, sizeof cached_path - 1);
    cached_path[sizeof cached_path - 1] = 0;
  }

  /* Metadata is looked up per redraw (a bsearch) rather than cached: the
   * Game pointer dies whenever the gamelist reloads. */
  const Game *g = settings_list_info ? gamelist_meta(full_path) : NULL;
  const char *gname = gl_get(g, "name"), *desc = gl_get(g, "desc"),
             *genre = gl_get(g, "genre"), *dev = gl_get(g, "developer"),
             *date = gl_get(g, "releasedate"), *players = gl_get(g, "players"),
             *rating = gl_get(g, "rating");
  int has_info = g && (desc[0] || genre[0] || dev[0] || date[0] ||
                       (players[0] && atoi(players) > 0) || rating[0]);
  if (!chas && !has_info)
    return; /* nothing to show → no panel */

  int lh = UI_S(18), nameh = UI_S(22);
  int art_h = has_info ? ph * 45 / 100 : ph - nameh;
  banner_fill_region(fb, px, top, pw, ph, COLOR_BG);
  if (chas)
    switcher_blit565(fb, ctb.data, ctb.alpha, ctb.width, ctb.height, px, top,
                     pw, art_h);

  /* Title, truncated to the panel width. */
  const char *title = gname[0] ? gname : name;
  char nm[64];
  int maxc = pw / UI_S(8);
  if (maxc > (int)sizeof(nm) - 1)
    maxc = sizeof(nm) - 1;
  int i = 0;
  for (; title[i] && i < maxc; i++)
    nm[i] = title[i];
  if (title[i]) {
    if (i > 2)
      i -= 2;
    nm[i++] = '.';
    nm[i++] = '.';
  }
  nm[i] = '\0';

  if (!has_info) { /* original behaviour: centered name under the art */
    int tw = (int)strlen(nm) * UI_S(8);
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, px + (pw - tw) / 2,
                   top + art_h + UI_S(4), nm, COLOR_TEXT);
    return;
  }

  int y = top + art_h + UI_S(4);
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, px, y, nm, COLOR_TEXT);
  y += lh;

  /* "2P | RPG | Nintendo | 1989 | 8.5/10" (ES rating is 0..1). */
  char info[128] = "";
  size_t o = 0;
  char pl[8] = "", yr[8] = "", rt[16] = "";
  if (players[0] && atoi(players) > 0)
    snprintf(pl, sizeof pl, "%dP", atoi(players));
  if (strlen(date) >= 4) {
    memcpy(yr, date, 4);
    yr[4] = '\0';
  }
  if (rating[0] && atof(rating) > 0.0)
    snprintf(rt, sizeof rt, "%.1f/10", atof(rating) * 10.0);
  const char *parts[] = {pl, genre, dev, yr, rt};
  for (size_t k = 0; k < sizeof parts / sizeof parts[0]; k++)
    if (parts[k][0] && o < sizeof info)
      o += (size_t)snprintf(info + o, sizeof info - o, "%s%s", o ? " | " : "",
                            parts[k]);
  if (info[0])
    y = draw_wrapped(fb, px, y, pw, lh, 2, info, COLOR_SELECT_TEXT);

  int lines_left = (bot - y) / lh;
  if (desc[0] && lines_left > 0)
    draw_wrapped(fb, px, y + UI_S(2), pw, lh, lines_left, desc, COLOR_TEXT);
}
