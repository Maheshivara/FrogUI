#include "app.h"
#include "stb_image.h"

/* internal helpers */
static float system_carousel_ease(void);
static int system_carousel_delta(int index);
static void fit_system_label(const char *source, char *dest, size_t size,
                             int max_width);
static SystemIcon *system_icon_cached(const char *path);
static const char *system_icon_path(const char *folder, char *path,
                                    size_t size);
static SystemIcon *system_icon_get(const char *folder);
static void system_icon_draw(uint16_t *fb, const SystemIcon *icon, int x, int y,
                             int w, int h);

int system_carousel_frame = SYSTEM_CAROUSEL_FRAMES + 1;
float system_carousel_start_offset = 0.0f;
float system_carousel_visible_offset = 0.0f;
bool horizontal_system_view(void) {
  return settings_style == STYLE_HORIZONTAL &&
         strcmp(current_path, ROMS_PATH) == 0 && !viewing_recents &&
         !viewing_favourites && !viewing_apps && !apps_browsing &&
         !viewing_search;
}
bool icon_system_view(void) {
  return settings_style == STYLE_SYSTEM &&
         strcmp(current_path, ROMS_PATH) == 0 && !viewing_recents &&
         !viewing_favourites && !viewing_apps && !apps_browsing &&
         !viewing_search;
}
bool system_carousel_is_animating(void) {
  return horizontal_system_view() && settings_anim &&
         system_carousel_frame <= SYSTEM_CAROUSEL_FRAMES;
}
void system_carousel_start(int direction) {
  if (settings_anim) {
    /* selected_index has already moved. Carry over the current visual
     * offset so a second tap does not restart the labels from a whole slot
     * away and visibly jump. */
    system_carousel_start_offset =
        system_carousel_visible_offset + (float)direction;
    system_carousel_frame = 0;
  } else {
    system_carousel_start_offset = 0.0f;
    system_carousel_visible_offset = 0.0f;
    system_carousel_frame = SYSTEM_CAROUSEL_FRAMES + 1;
  }
}
static float system_carousel_ease(void) {
  if (!settings_anim || system_carousel_frame > SYSTEM_CAROUSEL_FRAMES)
    return 1.0f;
  float p = (float)system_carousel_frame / (float)SYSTEM_CAROUSEL_FRAMES;
  return p * p * (3.0f - 2.0f * p);
}
static int system_carousel_delta(int index) {
  int delta = index - selected_index;
  if (entry_count > 1) {
    while (delta > entry_count / 2)
      delta -= entry_count;
    while (delta < -(entry_count / 2))
      delta += entry_count;
  }
  return delta;
}
static void fit_system_label(const char *source, char *dest, size_t size,
                             int max_width) {
  size_t keep = strlen(source);
  if (size < 5) {
    if (size > 0)
      dest[0] = '\0';
    return;
  }
  if (keep > size - 4)
    keep = size - 4;
  snprintf(dest, size, "%s", source);
  while (keep > 1 && font_measure_text(dest) > max_width) {
    keep--;
    snprintf(dest, size, "%.*s...", (int)keep, source);
  }
}
void render_system_carousel(uint16_t *fb) {
  render_tabs(fb, MAIN_TAB_GAMES, COLOR_BG);
  if (entry_count <= 0) {
    render_legend(fb, LEGEND_X_NONE, 0, 1);
    return;
  }

  int slot_width = SCREEN_WIDTH / 3;
  int center_x = SCREEN_WIDTH / 2;
  int text_y = SCREEN_HEIGHT * 3 / 5;
  float ease = system_carousel_ease();
  float offset = system_carousel_start_offset * (1.0f - ease);
  system_carousel_visible_offset = offset;

  for (int i = 0; i < entry_count; i++) {
    int delta = system_carousel_delta(i);
    float position = (float)delta + offset;
    if (position < -2.0f || position > 2.0f)
      continue;

    char label[96];
    fit_system_label(system_display_name(entries[i].name), label, sizeof label,
                     slot_width - UI_S(22));
    int text_width = font_measure_text(label);
    int x = center_x + (int)(position * slot_width) - text_width / 2;
    if (x + text_width < 0 || x >= SCREEN_WIDTH)
      continue;

    if (i == selected_index) {
      render_text_pillbox(fb, x, text_y, label, COLOR_SELECT_BG,
                          COLOR_SELECT_TEXT, UI_S(7), 0);
    } else {
      render_text_pillbox(fb, x, text_y, label, COLOR_LEGEND_BG, COLOR_DISABLED,
                          UI_S(7), 0);
    }
  }

  char count[32];
  snprintf(count, sizeof count, "%d / %d", selected_index + 1, entry_count);
  int count_x = (SCREEN_WIDTH - font_measure_text(count)) / 2;
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, count_x, text_y + ITEM_HEIGHT,
                 count, COLOR_HEADER);

  int show_select =
      strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) != 0;
  render_legend(fb, LEGEND_X_NONE, show_select, 1);

  if (system_carousel_frame <= SYSTEM_CAROUSEL_FRAMES)
    system_carousel_frame++;
}
SystemIcon system_icons[SYSTEM_ICON_CACHE_N];
static unsigned system_icon_clock;
int system_icon_pack = -1;
static char system_icon_missing[SYSTEM_ICON_MISSING_N][512];
int system_icon_missing_count;
static SystemIcon *system_icon_cached(const char *path) {
  for (int i = 0; i < SYSTEM_ICON_CACHE_N; i++)
    if (system_icons[i].pixels && strcmp(system_icons[i].path, path) == 0)
      return &system_icons[i];
  return NULL;
}
static const char *system_icon_path(const char *folder, char *path,
                                    size_t size) {
  if (!folder || !*folder)
    return NULL;
  const char *key = folder;
  if (strcmp(folder, FAVOURITES_ENTRY_NAME) == 0)
    key = "favourites";
  else if (strcmp(folder, RECENTS_ENTRY_NAME) == 0)
    key = "recents";
  else if (strcmp(folder, SETTINGS_ENTRY_NAME) == 0)
    key = "settings";
  /* Master System is stored as SMS on some cards, while the icon packs use
   * the shared Game Gear/Master System artwork key (gg). */
  else if (strcasecmp(folder, "sms") == 0)
    key = "gg";
  if (settings_icon_pack_idx > 0 && settings_icon_pack_idx < icon_pack_count)
    snprintf(path, size, BANNER_DIR "/icon-packs/%s/%s.png",
             icon_pack_files[settings_icon_pack_idx], key);
  else
    snprintf(path, size, BANNER_DIR "/system-icons/%s.png", key);
  if (system_icon_cached(path))
    return path;
  for (int i = 0; i < system_icon_missing_count; i++)
    if (strcmp(system_icon_missing[i], path) == 0)
      return NULL;
  if (access(path, R_OK) == 0)
    return path;
  if (system_icon_missing_count < SYSTEM_ICON_MISSING_N) {
    strncpy(system_icon_missing[system_icon_missing_count], path, 511);
    system_icon_missing[system_icon_missing_count][511] = '\0';
    system_icon_missing_count++;
  }
  return NULL;
}
static SystemIcon *system_icon_get(const char *folder) {
  if (system_icon_pack != settings_icon_pack_idx) {
    for (int i = 0; i < SYSTEM_ICON_CACHE_N; i++) {
      free(system_icons[i].pixels);
      free(system_icons[i].alpha);
      memset(&system_icons[i], 0, sizeof system_icons[i]);
    }
    system_icon_missing_count = 0;
    system_icon_pack = settings_icon_pack_idx;
  }
  char path[512];
  if (!system_icon_path(folder, path, sizeof path))
    return NULL;
  system_icon_clock++;
  SystemIcon *cached = system_icon_cached(path);
  if (cached) {
    cached->used = system_icon_clock;
    return cached;
  }

  int victim = 0;
  for (int i = 1; i < SYSTEM_ICON_CACHE_N; i++)
    if (!system_icons[i].pixels ||
        system_icons[i].used < system_icons[victim].used)
      victim = i;

  int sw, sh, channels;
  unsigned char *rgba = stbi_load(path, &sw, &sh, &channels, 4);
  if (!rgba || sw <= 0 || sh <= 0) {
    if (rgba)
      stbi_image_free(rgba);
    return NULL;
  }
  int dw = sw, dh = sh;
  if (dw > SYSTEM_ICON_MAX_W) {
    dh = dh * SYSTEM_ICON_MAX_W / dw;
    dw = SYSTEM_ICON_MAX_W;
  }
  if (dh > SYSTEM_ICON_MAX_H) {
    dw = dw * SYSTEM_ICON_MAX_H / dh;
    dh = SYSTEM_ICON_MAX_H;
  }
  if (dw < 1)
    dw = 1;
  if (dh < 1)
    dh = 1;
  uint16_t *pixels = malloc((size_t)dw * dh * sizeof(*pixels));
  uint8_t *alpha = malloc((size_t)dw * dh);
  if (!pixels || !alpha) {
    free(pixels);
    free(alpha);
    stbi_image_free(rgba);
    return NULL;
  }
  unsigned visible_pixels = 0;
  unsigned bright_pixels = 0;
  for (int y = 0; y < dh; y++) {
    int sy = y * sh / dh;
    for (int x = 0; x < dw; x++) {
      int sx = x * sw / dw;
      const unsigned char *p = rgba + ((size_t)sy * sw + sx) * 4;
      size_t dst = (size_t)y * dw + x;
      pixels[dst] =
          (uint16_t)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3));
      alpha[dst] = p[3];
      /* Ignore transparent fringe pixels. A pack is considered bright
       * when most of its visible glyph is pale enough to disappear on
       * a light selection pill. */
      if (p[3] >= 64) {
        unsigned luma = (299u * p[0] + 587u * p[1] + 114u * p[2]) / 1000u;
        visible_pixels++;
        if (luma >= 184)
          bright_pixels++;
      }
    }
  }
  stbi_image_free(rgba);

  free(system_icons[victim].pixels);
  free(system_icons[victim].alpha);
  system_icons[victim].pixels = pixels;
  system_icons[victim].alpha = alpha;
  system_icons[victim].width = dw;
  system_icons[victim].height = dh;
  system_icons[victim].bright =
      visible_pixels > 0 && bright_pixels * 100u >= visible_pixels * 60u;
  system_icons[victim].used = system_icon_clock;
  strncpy(system_icons[victim].path, path,
          sizeof system_icons[victim].path - 1);
  system_icons[victim].path[sizeof system_icons[victim].path - 1] = '\0';
  return &system_icons[victim];
}
static void system_icon_draw(uint16_t *fb, const SystemIcon *icon, int x, int y,
                             int w, int h) {
  if (!fb || !icon || !icon->pixels || !icon->alpha || w <= 0 || h <= 0)
    return;
  int dw = w, dh = icon->height * w / icon->width;
  if (dh > h) {
    dh = h;
    dw = icon->width * h / icon->height;
  }
  if (dw < 1 || dh < 1)
    return;
  int ox = x + (w - dw) / 2, oy = y + (h - dh) / 2;
  for (int dy = 0; dy < dh; dy++) {
    int sy = dy * icon->height / dh;
    uint16_t *dst = fb + (size_t)(oy + dy) * SCREEN_WIDTH + ox;
    for (int dx = 0; dx < dw; dx++) {
      int sx = dx * icon->width / dw;
      size_t src = (size_t)sy * icon->width + sx;
      unsigned a = icon->alpha[src];
      if (!a)
        continue;
      uint16_t color = icon->pixels[src];
      if (a == 255) {
        dst[dx] = color;
        continue;
      }
      unsigned sr = (color >> 11) & 31, sg = (color >> 5) & 63, sb = color & 31;
      unsigned dr = (dst[dx] >> 11) & 31, dg = (dst[dx] >> 5) & 63,
               db = dst[dx] & 31;
      dst[dx] = (uint16_t)((((sr * a + dr * (255 - a)) / 255) << 11) |
                           (((sg * a + dg * (255 - a)) / 255) << 5) |
                           ((sb * a + db * (255 - a)) / 255));
    }
  }
}
void render_system_grid(uint16_t *fb) {
  render_tabs(fb, MAIN_TAB_GAMES, COLOR_BG);
  if (entry_count <= 0) {
    render_legend(fb, LEGEND_X_NONE, 0, 1);
    return;
  }

  enum { COLS = 4, ROWS = 2, PAGE_SIZE = COLS * ROWS };
  int page = selected_index / PAGE_SIZE;
  int pages = (entry_count + PAGE_SIZE - 1) / PAGE_SIZE;
  int first = page * PAGE_SIZE;
  int top = HEADER_HEIGHT + UI_S(8);
  int bottom = SCREEN_HEIGHT - ITEM_HEIGHT - UI_S(25);
  int grid_h = bottom - top;
  int gap = UI_S(8);
  int cell_w = (SCREEN_WIDTH - PADDING * 2 - gap * (COLS - 1)) / COLS;
  int cell_h = (grid_h - gap * (ROWS - 1)) / ROWS;

  for (int slot = 0; slot < PAGE_SIZE; slot++) {
    int idx = first + slot;
    if (idx >= entry_count)
      break;
    int col = slot % COLS, row = slot / COLS;
    int x = PADDING + col * (cell_w + gap);
    int y = top + row * (cell_h + gap);
    int active = idx == selected_index;

    SystemIcon *icon = system_icon_get(entries[idx].name);
    uint16_t tile_bg = COLOR_SELECT_BG;
    uint16_t tile_text = COLOR_SELECT_TEXT;
    if (active && icon && icon->bright) {
      unsigned bg_r = ((tile_bg >> 11) & 31u) * 255u / 31u;
      unsigned bg_g = ((tile_bg >> 5) & 63u) * 255u / 63u;
      unsigned bg_b = (tile_bg & 31u) * 255u / 31u;
      unsigned bg_luma = (299u * bg_r + 587u * bg_g + 114u * bg_b) / 1000u;
      if (bg_luma >= 160u) {
        /* Local contrast fix: invert only this selected system tile.
         * Menus, legends and every other themed pill stay unchanged. */
        tile_bg = COLOR_SELECT_TEXT;
        tile_text = COLOR_SELECT_BG;
      }
    }
    if (active)
      render_rounded_rect(fb, x, y, cell_w, cell_h, UI_S(10), tile_bg);

    const char *shown = system_display_name(entries[idx].name);
    char label[96];
    fit_system_label(shown, label, sizeof label, cell_w - UI_S(12));
    int label_w = font_measure_text(label);
    int label_y = y + cell_h - ITEM_HEIGHT + UI_S(5);
    if (icon) {
      system_icon_draw(fb, icon, x + UI_S(12), y + UI_S(10), cell_w - UI_S(24),
                       cell_h - ITEM_HEIGHT - UI_S(8));
    } else {
      /* Custom folders remain useful even without an icon pack entry. */
      char initials[4] = {0};
      initials[0] = shown[0] ? shown[0] : '?';
      initials[1] = shown[1] ? shown[1] : '\0';
      for (int i = 0; initials[i]; i++)
        if (initials[i] >= 'a' && initials[i] <= 'z')
          initials[i] -= 32;
      int iw = font_measure_text(initials);
      font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, x + (cell_w - iw) / 2,
                     y + cell_h / 2 - UI_S(8), initials,
                     active ? tile_text : COLOR_TEXT);
    }
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, x + (cell_w - label_w) / 2,
                   label_y, label, active ? tile_text : COLOR_TEXT);
  }

  char count[32];
  snprintf(count, sizeof count, "%d / %d", page + 1, pages);
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT,
                 (SCREEN_WIDTH - font_measure_text(count)) / 2,
                 bottom + UI_S(4), count, COLOR_HEADER);
  int show_select =
      strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) != 0;
  render_legend(fb, LEGEND_X_NONE, show_select, 1);
}
