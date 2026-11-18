#include "app.h"
#include "stb_image.h"

/* internal helpers */
static int switcher_savestate_bmp(const char *full_path, char *out, size_t n);
static void render_game_switcher_header(uint16_t *framebuffer, int barh);

/* Total seconds played for a game, from picoarch's playtime.txt. */
long playtime_lookup(const char *path) {
  if (!path || !*path)
    return 0;
  FILE *f = fopen("/mnt/sdcard/frogui/playtime.txt", "r");
  if (!f)
    return 0;
  char line[1100];
  long sec = 0;
  while (fgets(line, sizeof line, f)) {
    char *t = strchr(line, '\t');
    if (!t)
      continue;
    *t = 0;
    char *p = t + 1;
    p[strcspn(p, "\r\n")] = 0;
    if (!strcmp(p, path)) {
      sec = atol(line);
      break;
    }
  }
  fclose(f);
  return sec;
}
/* Newest save-state screenshot for a game:
 * /mnt/sdcard/picoarch/<tag>/<base>.st<N>.bmp */
static int switcher_savestate_bmp(const char *full_path, char *out, size_t n) {
  char dir[640];
  strncpy(dir, full_path, sizeof dir - 1);
  dir[sizeof dir - 1] = 0;
  char *sl = strrchr(dir, '/');
  if (!sl)
    return 0;
  char base[256];
  strncpy(base, sl + 1, sizeof base - 1);
  base[sizeof base - 1] = 0;
  *sl = 0;
  char *tagsl = strrchr(dir, '/');
  const char *tag = tagsl ? tagsl + 1 : dir;
  char *dot = strrchr(base, '.');
  if (dot)
    *dot = 0; /* strip extension */
  /* The ROM browser preserves the card's folder spelling (often NES),
   * while picoarch's config tag is normalized to lowercase (nes).  Probe
   * both forms; otherwise valid .scr.bmp files silently fall back to art. */
  char lower_tag[sizeof base];
  size_t tag_len = strlen(tag);
  if (tag_len >= sizeof lower_tag)
    tag_len = sizeof lower_tag - 1;
  for (size_t i = 0; i < tag_len; i++)
    lower_tag[i] = (char)tolower((unsigned char)tag[i]);
  lower_tag[tag_len] = '\0';
  const char *tags[2] = {tag, lower_tag};
  for (int ti = 0; ti < 2; ti++) {
    if (ti == 1 && !strcasecmp(tags[0], tags[1]))
      continue;
    /* dedicated per-game last-screen snapshot (written on menu entry) */
    snprintf(out, n, "/mnt/sdcard/picoarch/%s/%s.scr.bmp", tags[ti], base);
    if (access(out, F_OK) == 0) {
      dbg("recents: using gameplay screenshot");
      return 1;
    }
    for (int slot = 9; slot >= 0; slot--) { /* then save states */
      snprintf(out, n, "/mnt/sdcard/picoarch/%s/%s.st%d.bmp", tags[ti], base,
               slot);
      if (access(out, F_OK) == 0) {
        dbg("recents: using save-state screenshot");
        return 1;
      }
    }
  }
  dbg("recents: no screenshot found");
  return 0;
}
void switcher_blit565(uint16_t *fb, const uint16_t *src, const uint8_t *alpha,
                      int sw, int sh, int bx, int by, int bw, int bh) {
  if (!src || sw <= 0 || sh <= 0)
    return;
  int dw = bw, dh = sh * bw / sw;
  if (dh > bh) {
    dh = bh;
    dw = sw * bh / sh;
  }
  int ox = bx + (bw - dw) / 2, oy = by + (bh - dh) / 2;
  for (int y = 0; y < dh; y++) {
    int sy = y * sh / dh;
    const uint16_t *r = src + (size_t)sy * sw;
    const uint8_t *ar = alpha ? alpha + (size_t)sy * sw : NULL;
    uint16_t *d = fb + (size_t)(oy + y) * SCREEN_WIDTH + ox;
    for (int x = 0; x < dw; x++) {
      int sx = x * sw / dw;
      unsigned a = ar ? ar[sx] : 255;
      if (a == 0)
        continue;
      uint16_t px = r[sx];
      if (a == 255) {
        d[x] = px;
        continue;
      }
      unsigned sr = (px >> 11) & 0x1F, sg = (px >> 5) & 0x3F, sb = px & 0x1F;
      unsigned dr = (d[x] >> 11) & 0x1F, dg = (d[x] >> 5) & 0x3F,
               db = d[x] & 0x1F;
      d[x] = (uint16_t)((((sr * a + dr * (255 - a)) / 255) << 11) |
                        (((sg * a + dg * (255 - a)) / 255) << 5) |
                        ((sb * a + db * (255 - a)) / 255));
    }
  }
}
static void render_game_switcher_header(uint16_t *framebuffer, int barh) {
  render_fill_rect(framebuffer, 0, 0, SCREEN_WIDTH, barh, COLOR_LEGEND_BG);
  render_tabs(framebuffer, MAIN_TAB_RECENTS, COLOR_LEGEND_BG);
}
void render_game_switcher(uint16_t *framebuffer) {
  const RecentGame *list = recent_games_get_list();
  int n = recent_games_get_count();
  int barh = UI_S(30);
  if (n <= 0) {
    render_game_switcher_header(framebuffer, barh);
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                   SCREEN_HEIGHT / 2, "No recent games yet", COLOR_TEXT);
    render_legend(framebuffer, LEGEND_X_NONE, 0, 0);
    return;
  }
  if (selected_index < 0)
    selected_index = 0;
  if (selected_index >= n)
    selected_index = n - 1;
  const RecentGame *g = &list[selected_index];

  /* Onion-style: keep the capture at full panel geometry in both modes and
   * paint UI on top. Resizing around the bars distorts gameplay captures. */
  int bx = 0, by = 0;
  int bw = SCREEN_WIDTH, bh = SCREEN_HEIGHT;
  /* Decoding PNG/JPG/BMP from FAT is the expensive part of entering Recents.
   * Compose the selected capture to panel-sized RGB565 once, then every
   * transition/redraw is only a memcpy. */
  static uint16_t *cached_frame = NULL;
  static int cached_pixels = 0, cached_drawn = 0;
  static char cached_game[MAX_PATH_LEN] = "";
  int pixels = bw * bh;
  if (!cached_frame || cached_pixels != pixels ||
      strcmp(cached_game, g->full_path) != 0) {
    if (cached_pixels != pixels) {
      free(cached_frame);
      cached_frame = (uint16_t *)malloc((size_t)pixels * sizeof(uint16_t));
      cached_pixels = cached_frame ? pixels : 0;
    }
    cached_drawn = 0;
    if (cached_frame)
      memset(cached_frame, 0, (size_t)pixels * sizeof(uint16_t));

    char path[1024];
    Thumbnail tb;
    /* A Recents card should show where the player left off. Only use
     * curated artwork when no gameplay capture exists yet. */
    if (cached_frame &&
        switcher_savestate_bmp(g->full_path, path, sizeof path)) {
      int w, h, ch;
      unsigned char *img = stbi_load(path, &w, &h, &ch, 3);
      if (img) {
        /* Gameplay captures use the core's raw pixel geometry. PS1 in
         * particular can produce 256/320/368/512/640-wide modes whose
         * pixels are meant to be stretched by the display, not shown
         * square. Fill the switcher viewport just like live gameplay;
         * aspect-preserving fit made 640x240 captures look squashed into
         * a wide strip. Box art still uses switcher_blit565() above. */
        for (int y = 0; y < bh; y++) {
          const unsigned char *rr = img + (size_t)(y * h / bh) * w * 3;
          uint16_t *d = cached_frame + (size_t)(by + y) * SCREEN_WIDTH + bx;
          for (int x = 0; x < bw; x++) {
            const unsigned char *p = rr + (size_t)(x * w / bw) * 3;
            d[x] = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
          }
        }
        stbi_image_free(img);
        cached_drawn = 1;
      }
    }
    if (cached_frame && !cached_drawn &&
        game_art_load(g->full_path, ARTWORK_BOXART, &tb)) {
      switcher_blit565(cached_frame, tb.data, tb.alpha, tb.width, tb.height, bx,
                       by, bw, bh);
      free_thumbnail(&tb);
      cached_drawn = 1;
    }
    if (cached_frame && !cached_drawn &&
        game_art_load(g->full_path, ARTWORK_TITLE_SCREEN, &tb)) {
      switcher_blit565(cached_frame, tb.data, tb.alpha, tb.width, tb.height, bx,
                       by, bw, bh);
      free_thumbnail(&tb);
      cached_drawn = 1;
    }
    strncpy(cached_game, g->full_path, sizeof cached_game - 1);
    cached_game[sizeof cached_game - 1] = '\0';
  }
  if (cached_frame)
    memcpy(framebuffer, cached_frame, (size_t)pixels * sizeof(uint16_t));
  else
    render_fill_rect(framebuffer, bx, by, bw, bh, 0x0000);
  if (!cached_drawn)
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, bx + UI_S(16),
                   by + bh / 2, "(no screenshot - open the in-game menu once)",
                   COLOR_TEXT);

  /* Bottom overlay: detailed mode carries play info; fullscreen keeps only
   * the selected game and navigation, matching Onion's minimal view. */
  int byb = SCREEN_HEIGHT - barh;
  render_fill_rect(framebuffer, 0, byb, SCREEN_WIDTH, barh, COLOR_SELECT_BG);
  if (game_switcher_fullscreen) {
    int nw = font_measure_text(g->game_name);
    int nx = (SCREEN_WIDTH - nw) / 2;
    if (selected_index > 0)
      font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                     byb + UI_S(8), "<", COLOR_SELECT_TEXT);
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, nx, byb + UI_S(8),
                   g->game_name, COLOR_SELECT_TEXT);
    if (selected_index + 1 < n)
      font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                     SCREEN_WIDTH - PADDING - font_measure_text(">"),
                     byb + UI_S(8), ">", COLOR_SELECT_TEXT);
  } else {
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                   byb + UI_S(8), g->game_name, COLOR_SELECT_TEXT);
    char info[96];
    long secs = playtime_lookup(g->full_path);
    if (secs >= 3600)
      snprintf(info, sizeof info, "Played %ldh %ldm   %d/%d", secs / 3600,
               (secs % 3600) / 60, selected_index + 1, n);
    else if (secs >= 60)
      snprintf(info, sizeof info, "Played %ldm   %d/%d", secs / 60,
               selected_index + 1, n);
    else if (secs > 0)
      snprintf(info, sizeof info, "Played %lds   %d/%d", secs,
               selected_index + 1, n);
    else
      snprintf(info, sizeof info, "%d/%d", selected_index + 1, n);
    int iw = font_measure_text(info);
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                   SCREEN_WIDTH - iw - PADDING, byb + UI_S(8), info,
                   COLOR_SELECT_TEXT);
    render_game_switcher_header(framebuffer, barh);
  }
}
