#include "../app/app.h"

/* internal helpers */
static bool settings_row_enabled(const SRow *r);
static int settings_section_of(int i);
static void settings_sections_save(void);
static int settings_vis_cursor(void);
static bool settings_row_visible(int i);
static int settings_row_selectable(int i);
static void settings_preview_row(const SRow *r);

const SRow settings_rows[] = {
    {RT_HEADER, "settings.appearance"},
    {RT_THEME, "settings.theme"},
    {RT_THEME_PACK, "settings.background_theme_pack"},
    {RT_STYLE, "settings.style"},
    {RT_ICON_PACK, "settings.icon_pack"},
    {RT_TOGGLE, "settings.center_text", &settings_center_text},
    {RT_TOGGLE, "settings.friendly_system_names", &settings_friendly_names},
    {RT_FONT, "settings.font"},
    {RT_RANGE, "settings.font_size", &settings_font_size, 18, 26, 1},
    {RT_TOGGLE, "settings.battery_colour_mode", &settings_battery_color},
    {RT_TOGGLE, "settings.background_images", &settings_backgrounds},
    {RT_RANGE, "settings.background_dim", &settings_background_dim, 0, 100, 5},
    {RT_WALLPAPER, "settings.wallpaper"},
    {RT_WALLFIT, "settings.background_image_fit"},
    {RT_HEADER, "settings.general"},
    {RT_LANGUAGE, "settings.language", &settings_language},
    {RT_RANGE, "settings.brightness", &settings_brightness, 0, 100,
     SETTINGS_BRIGHTNESS_STEP},
    {RT_TOGGLE, "settings.animations", &settings_anim},
    {RT_TOGGLE, "settings.menu_sounds", &settings_menu_sounds},
    {RT_TOGGLE, "settings.hide_extensions", &settings_hide_extensions},
    {RT_TOGGLE, "settings.hide_empty_folders", &settings_hide_empty},
    {RT_HEADER, "settings.library"},
    {RT_ROM_SOURCE, "settings.rom_source"},
    {RT_OTG_STATUS, "settings.otg_storage"},
    {RT_TOGGLE, "settings.gamelist_scan_dir", &settings_gamelist_dir},
    {RT_TOGGLE, "settings.list_game_info", &settings_list_info},
    {RT_TOGGLE, "settings.game_switcher", &settings_game_switcher},
    {RT_TOGGLE, "settings.start_in_recents", &settings_load_recents},
    {RT_HEADER, "settings.gameplay"},
    {RT_TOGGLE, "settings.quick_resume", &settings_quick_resume},
    {RT_TOGGLE, "settings.autosave_autoload", &settings_autosave_autoload},
    {RT_TOGGLE, "settings.custom_aspect_ratios",
     &settings_custom_aspect_ratios},
    {RT_HEADER, "settings.system"},
    {RT_RANGE, "settings.volume", &settings_volume, 0, 100, 5},
    {RT_TOGGLE, "settings.file_cache", &settings_file_cache},
    {RT_CACHE_REBUILD, "settings.rebuild_file_cache"},
    {RT_TOGGLE, "settings.disable_sleep", &settings_disable_sleep},
    {RT_ACTION, "settings.button_mapping"},
    {RT_INFO, "settings.version"},
};
#undef SETTINGS_ROW_N
#define SETTINGS_ROW_N ((int)(sizeof(settings_rows) / sizeof(settings_rows[0])))
const int settings_row_count = SETTINGS_ROW_N;
/* A disabled dependent row stays visible (so the relationship is obvious),
 * but cannot be changed until its prerequisite is enabled. */
static bool settings_row_enabled(const SRow *r) {
  if (!r)
    return false;
  if (r->type == RT_THEME_PACK)
    return settings_backgrounds != 0;
  if (r->type == RT_ICON_PACK)
    return settings_style == STYLE_SYSTEM;
  return true;
}
/* ---- Collapsible settings sections ---------------------------------------
 * Headers stay dividers for rendering, but the cursor can land on them and
 * LEFT/RIGHT (or A) collapses/expands their section — same interaction as the
 * SELECT picker's ">> Cores" / ">> EXTENSIONS". The visible-row list below
 * is rebuilt whenever a section flips. */
static const char *settings_section_names[] = {
    "settings.appearance", "settings.general", "settings.library",
    "settings.gameplay",   "settings.system",  NULL};
bool settings_section_open[SETTINGS_SECTION_N] = {true, true, true, true, true};
/* Map a settings_rows[] header index -> section id (index into
 * settings_section_names), or -1 when the row is not a known header. */
static int settings_section_of(int i) {
  if (i < 0 || i >= SETTINGS_ROW_N || settings_rows[i].type != RT_HEADER)
    return -1;
  for (int s = 0; settings_section_names[s]; s++)
    if (strcmp(settings_rows[i].label, settings_section_names[s]) == 0)
      return s;
  return -1;
}
/* ---- Collapsed-state persistence (review feedback) -----------------------
 * Each category's collapsed/expanded state is stored in the regular
 * settings file as one compact line ("sections=01011", one digit per
 * section, 0=collapsed 1=expanded). Default when absent: all expanded.
 * Saved immediately when the user toggles a section, so the layout comes
 * back exactly as it was left across reboots and updates. */
static void settings_sections_save(void) {
  /* Reuse the same file: read the current contents, replace/append the
   * sections line, write back. The file is tiny (a few hundred bytes). */
  char buf[2048];
  size_t len = 0;
  FILE *f = fopen(SETTINGS_FILE, "r");
  if (f) {
    len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
  }
  buf[len] = '\0';

  char newline[32];
  snprintf(newline, sizeof newline, "sections=%d%d%d%d%d",
           settings_section_open[0] ? 1 : 0, settings_section_open[1] ? 1 : 0,
           settings_section_open[2] ? 1 : 0, settings_section_open[3] ? 1 : 0,
           settings_section_open[4] ? 1 : 0);

  FILE *out = fopen(SETTINGS_FILE, "w");
  if (!out)
    return;
  int wrote = 0;
  char *line = buf;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    if (strncmp(line, "sections=", 9) == 0) {
      fputs(newline, out);
      fputc('\n', out);
      wrote = 1;
    } else if (line[0] != '\0' || nl) {
      fputs(line, out);
      fputc('\n', out);
    }
    line = nl ? nl + 1 : NULL;
  }
  if (!wrote) {
    fputs(newline, out);
    fputc('\n', out);
  }
  fclose(out);
}
void settings_sections_load(void) {
  FILE *f = fopen(SETTINGS_FILE, "r");
  if (!f)
    return;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    if (strncmp(line, "sections=", 9) == 0) {
      const char *d = line + 9;
      for (int s = 0; s < SETTINGS_SECTION_N && d[s]; s++)
        settings_section_open[s] = (d[s] == '0') ? false : true;
      break;
    }
  }
  fclose(f);
}
/* Visible settings rows: every header plus, per open section, its option
 * rows. Built on demand; holds indices into settings_rows[]. */
int settings_vis_rows[SETTINGS_ROW_N];
int settings_vis_n = 0;
void settings_build_vis_rows(void) {
  int n = 0;
  for (int i = 0; i < SETTINGS_ROW_N; i++) {
    int sec = settings_section_of(i);
    if (sec >= 0) {
      settings_vis_rows[n++] = i; /* header always visible */
      if (!settings_section_open[sec]) {
        /* skip the section's rows in one sweep */
        while (i + 1 < SETTINGS_ROW_N && settings_section_of(i + 1) < 0)
          i++;
      }
    } else {
      settings_vis_rows[n++] = i;
    }
  }
  settings_vis_n = n;
}
/* Cursor position in the visible list; kept in sync with settings_menu_idx
 * (which stays an index into settings_rows[]). */
static int settings_vis_cursor(void) {
  for (int i = 0; i < settings_vis_n; i++)
    if (settings_vis_rows[i] == settings_menu_idx)
      return i;
  return 0;
}
static bool settings_row_visible(int i) {
  /* A row is visible unless it sits inside a COLLAPSED section: find the
   * nearest header above it; if that section is collapsed, the row is
   * hidden (headers themselves always stay visible). */
  if (i < 0 || i >= SETTINGS_ROW_N)
    return false;
  if (settings_section_of(i) >= 0)
    return true; /* header */
  for (int s = i - 1; s >= 0; s--) {
    int sec = settings_section_of(s);
    if (sec >= 0)
      return settings_section_open[sec];
  }
  return true; /* rows before any header: always visible */
}
static int settings_row_selectable(int i) {
  if (i < 0 || i >= SETTINGS_ROW_N)
    return 0;
  if (settings_rows[i].type == RT_ROM_SOURCE)
    return otg_roms_available();
  /* Headers are selectable now (they collapse/expand sections). */
  if (settings_rows[i].type == RT_HEADER) {
    int sec = settings_section_of(i);
    return sec >= 0;
  }
  return settings_rows[i].type != RT_INFO &&
         settings_rows[i].type != RT_OTG_STATUS;
}
static void settings_preview_row(const SRow *r) {
  if (!r)
    return;
  switch (r->type) {
  case RT_THEME:
    theme_apply(settings_theme_idx);
    theme_sync_artwork_pack();
    break;
  case RT_FONT:
    if (font_count > 0)
      font_load_file(font_files[settings_font_idx]);
    break;
  case RT_RANGE:
    if (r->val == &settings_font_size)
      font_set_size(settings_font_size);
    else if (r->val == &settings_brightness)
      cube_set_backlight(settings_brightness);
    else if (r->val == &settings_background_dim)
      banner_set_dim(settings_background_dim);
    else if (r->val == &settings_volume) {
      /* Live on EVERY step: the hardware mirror makes the change
       * audible immediately, and the debounced commit keeps the
       * stored value (physical meter) following the slider in
       * real time without per-step EEPROM writes. */
      cube_volume_preview(settings_volume);
      settings_volume_commit_live();
    }
    break;
  case RT_TOGGLE:
    if (r->val == &settings_anim)
      banner_set_anim(settings_anim);
    break;
  default:
    break;
  }
}
void handle_settings_menu(void) {
  extern const int theme_count;
  /* Settings used to use edge-only presses while the browser used the
   * time-based repeat path. Keep both menus equally responsive, including
   * smooth scrolling/adjusting while a direction is held. */
  bool up = input_repeat(FROG_BTN_UP);
  bool down = input_repeat(FROG_BTN_DOWN);
  bool left = input_repeat(FROG_BTN_LEFT);
  bool right = input_repeat(FROG_BTN_RIGHT);

  settings_build_vis_rows();
  /* Keep the cursor on a visible row (rows hidden by a collapsed section
   * can no longer be selected, so it can drift off the list). */
  int cur = settings_vis_cursor();
  if (settings_vis_rows[cur] != settings_menu_idx) {
    settings_menu_idx = settings_vis_rows[0];
    cur = 0;
  }
  if (up) {
    cur = (cur - 1 + settings_vis_n) % settings_vis_n;
    settings_menu_idx = settings_vis_rows[cur];
  }
  if (down) {
    cur = (cur + 1) % settings_vis_n;
    settings_menu_idx = settings_vis_rows[cur];
  }
  int sec = settings_section_of(settings_menu_idx);
  if ((left || right) && sec >= 0) {
    /* LEFT/RIGHT on a ">>" header collapses/expands its section, same
     * interaction as the SELECT picker sections. */
    settings_section_open[sec] = left ? !settings_section_open[sec] : true;
    settings_sections_save(); /* persist immediately (review request) */
    settings_build_vis_rows();
    /* cursor stays on the header we just flipped */
  } else if ((left || right) &&
             settings_row_enabled(&settings_rows[settings_menu_idx])) {
    int delta = right ? 1 : -1;
    const SRow *r = &settings_rows[settings_menu_idx];
    switch (r->type) {
    case RT_THEME:
      settings_theme_idx =
          (settings_theme_idx + delta + theme_count) % theme_count;
      break;
    case RT_STYLE:
      settings_style = (settings_style + delta + STYLE_COUNT) % STYLE_COUNT;
      break;
    case RT_FONT:
      if (font_count > 0)
        settings_font_idx =
            (settings_font_idx + delta + font_count) % font_count;
      break;
    case RT_WALLPAPER:
      if (wallpaper_count > 0)
        settings_wallpaper_idx =
            (settings_wallpaper_idx + delta + wallpaper_count) %
            wallpaper_count;
      break;
    case RT_WALLFIT:
      settings_wallpaper_fit =
          (settings_wallpaper_fit + delta + WALL_FIT_N) % WALL_FIT_N;
      break;
    case RT_THEME_PACK:
      settings_theme_pack_idx =
          (settings_theme_pack_idx + delta + theme_pack_count) %
          theme_pack_count;
      break;
    case RT_ICON_PACK:
      settings_icon_pack_idx =
          (settings_icon_pack_idx + delta + icon_pack_count) % icon_pack_count;
      break;
    case RT_ROM_SOURCE:
      settings_rom_source =
          (settings_rom_source + delta + ROM_SOURCE_COUNT) % ROM_SOURCE_COUNT;
      break;
    case RT_LANGUAGE:
      settings_language = (settings_language + delta + i18n_language_count()) %
                          i18n_language_count();
      /* Locale changes need a fresh FrogUI core instance. Reinitializing
       * libretro from inside retro_run tears down picoarch itself, so
       * leave a one-shot boot flag and ask the normal launcher loop to
       * start us again. The new instance loads the saved locale before
       * scanning or rendering any system labels. */
      if (!i18n_init(i18n_language_code_at(settings_language))) {
        settings_language = 0;
        i18n_init(i18n_language_code_at(settings_language));
      }
      settings_save_file();
      {
        FILE *flag = fopen(LANGUAGE_RESTART_FLAG, "w");
        if (flag)
          fclose(flag);
      }
      if (environ_cb)
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
    case RT_TOGGLE:
      *r->val = (*r->val + delta + 2) % 2;
      break;
    case RT_RANGE:
      *r->val += delta * r->rstep;
      if (*r->val < r->rmin)
        *r->val = r->rmin;
      if (*r->val > r->rmax)
        *r->val = r->rmax;
      break;
    default:
      break;
    }
    settings_preview_row(r);
  }
  if (input_was_pressed(FROG_BTN_A) && sec >= 0) {
    /* A on a header flips its section (same as LEFT) and stays in the
     * menu — it must not fall through to the save/close path below. */
    settings_section_open[sec] = !settings_section_open[sec];
    settings_sections_save(); /* persist immediately (review request) */
    settings_build_vis_rows();
    return;
  }
  if (input_was_pressed(FROG_BTN_A) &&
      settings_rows[settings_menu_idx].type == RT_CACHE_REBUILD) {
    /* Rebuilding opts in, removes every old/corrupt cache entry, and
     * immediately regenerates the root library index. System folders are
     * then refreshed lazily the first time they are opened. */
    settings_file_cache = 1;
    cache_clear();
    scan_directory(ROMS_PATH);
    settings_save_file();
    strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
    current_path[MAX_PATH_LEN - 1] = '\0';
    selected_index = 0;
    scroll_offset = 0;
    settings_menu_active = false;
    save_games_tab_entries();
    ui_toast_show("File cache rebuilt");
    return;
  }
  if (input_was_pressed(FROG_BTN_A) &&
      settings_rows[settings_menu_idx].type == RT_ACTION) {
    remap_step = 0;
    remap_prev_raw = input_get_raw_state();
    remap_wizard_active = true;
    settings_menu_active = false;
    dbg("remap: start from settings");
    return;
  }
  if ((input_was_pressed(FROG_BTN_A) &&
       settings_rows[settings_menu_idx].type != RT_ACTION) ||
      input_was_pressed(FROG_BTN_B)) {
    settings_save_file();
    settings_volume_commit_final(); /* reset the live-commit debounce state */
    settings_menu_active = false;
    /* re-scan root so a hide-empty-folders change takes effect now */
    resolve_roms_root();
    scan_directory(ROMS_PATH);
    strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
    selected_index = 0;
    scroll_offset = 0;
    save_games_tab_entries();
  }
}
void render_settings_menu(void) {
  extern const Theme themes[];
  if (banner_is_loaded())
    banner_render(framebuffer);
  else
    render_clear_screen(framebuffer);
  render_tabs(framebuffer, MAIN_TAB_SETTINGS, COLOR_BG);

  char line[128];
  /* Scroll window over the VISIBLE rows (collapsed sections shrink the
   * list) so the selected row always stays on screen. */
  settings_build_vis_rows();
  int cur = settings_vis_cursor();
  static int soff = 0;
  if (cur < soff)
    soff = cur;
  if (cur >= soff + VISIBLE_ENTRIES)
    soff = cur - VISIBLE_ENTRIES + 1;
  if (soff > settings_vis_n - VISIBLE_ENTRIES)
    soff = settings_vis_n - VISIBLE_ENTRIES;
  if (soff < 0)
    soff = 0;

  int vis = settings_vis_n - soff;
  if (vis > VISIBLE_ENTRIES)
    vis = VISIBLE_ENTRIES;
  for (int i = 0; i < vis; i++) {
    int idx = settings_vis_rows[soff + i];
    const SRow *r = &settings_rows[idx];
    int y = START_Y + i * ITEM_HEIGHT;
    if (r->type == RT_HEADER) {
      /* Category divider: TreeFrogUI ">> " marker, accent color, no
       * pillbox when idle ??? but selectable, so pillbox under cursor. */
      snprintf(line, sizeof line, ">> %s", tr(r->label));
      if (settings_menu_idx == idx)
        render_text_pillbox(framebuffer, PADDING, y, line, COLOR_SELECT_BG,
                            COLOR_SELECT_TEXT, 7, 0);
      else
        font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y,
                       line, COLOR_SELECT_BG);
      continue;
    }
    switch (r->type) {
    case RT_INFO:
      snprintf(line, sizeof line, "%s: %s", tr(r->label), treefrogui_version());
      break;
    case RT_THEME:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               themes[settings_theme_idx].name);
      break;
    case RT_STYLE:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               style_names[settings_style]);
      break;
    case RT_FONT:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               font_count > 0 ? font_disp[settings_font_idx] : "(none)");
      break;
    case RT_WALLPAPER:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               wallpaper_disp[settings_wallpaper_idx]);
      break;
    case RT_WALLFIT:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               wallpaper_fit_names[settings_wallpaper_fit]);
      break;
    case RT_THEME_PACK:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               theme_pack_disp[settings_theme_pack_idx]);
      break;
    case RT_ICON_PACK:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               icon_pack_disp[settings_icon_pack_idx]);
      break;
    case RT_LANGUAGE: {
      char key[32];
      snprintf(key, sizeof key, "language.%s",
               i18n_language_code_at(settings_language));
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label), tr(key));
      break;
    }
    case RT_ROM_SOURCE:
      if (otg_roms_available())
        snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
                 rom_source_names[settings_rom_source]);
      else
        snprintf(line, sizeof line, "%s: %s", tr(r->label),
                 rom_source_names[ROM_SOURCE_SD]);
      break;
    case RT_OTG_STATUS:
      snprintf(
          line, sizeof line, "%s: %s", tr(r->label),
          tr(otg_roms_available() ? "value.connected" : "value.not_connected"));
      break;
    case RT_TOGGLE:
      snprintf(line, sizeof line, "%s: < %s >", tr(r->label),
               tr(*r->val ? "value.on" : "value.off"));
      break;
    case RT_RANGE:
      snprintf(line, sizeof line,
               r->val == &settings_font_size ? "%s: < %d px >" : "%s: < %d%% >",
               tr(r->label), *r->val);
      break;
    default:
      snprintf(line, sizeof line, "%s", tr(r->label));
      break; /* RT_ACTION */
    }
    /* Options sit indented under their ">> HEADER" so the grouping reads
     * clearly. Headers stay flush at PADDING. */
    int ix = PADDING + UI_S(16);
    if (settings_menu_idx == idx && settings_row_enabled(r))
      render_text_pillbox(framebuffer, ix, y, line, COLOR_SELECT_BG,
                          COLOR_SELECT_TEXT, 7, 0);
    else {
      int color = (!settings_row_enabled(r) || r->type == RT_OTG_STATUS ||
                   (r->type == RT_ROM_SOURCE && !otg_roms_available()))
                      ? COLOR_DISABLED
                      : COLOR_TEXT;
      font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, ix, y, line,
                     color);
    }
  }
  render_scroll_indicator(framebuffer, settings_vis_n, cur, VISIBLE_ENTRIES);
  render_legend(framebuffer, LEGEND_X_NONE, 0, 0);
}
