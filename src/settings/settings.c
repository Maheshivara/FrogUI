#include "app.h"

/* internal helpers */
static void mkdir_p(const char *path);
static void settings_write_volume(void);

int settings_wallpaper_idx = 0; /* 0 = none/per-system */
int settings_wallpaper_fit =
    2; /* BANNER_FIT_* (Fill/Fit/Stretch/Center/Tile) */
int settings_theme_pack_idx = 0;
int settings_icon_pack_idx = 0;
bool settings_menu_active = false;
/* Bump this for a runtime change that alters rendered strings but not the
 * browser's directory state.  The idle renderer otherwise intentionally keeps
 * presenting its last frame. */
int settings_menu_idx = 0; /* row: 0=theme, 1=font, 2=brightness, 3=quick
                              resume, 4=auto-save/auto-load, 5=animations... */
int settings_theme_idx = 0;
int settings_font_idx = 0;
int settings_font_size = 23;
int settings_brightness = 75; /* 0..100, step 5 */
/* Frames left to re-assert brightness after a cubevol (re)start. cubevol
 * applies its OWN stored brightness on start, DELAYED by panel/backlight-delay
 * (to avoid a boot flash) — which lands after our one-shot settings_apply and
 * overrides it (default brightness on cold boot / return from game).
 * Re-asserting over a short window outlasts that delayed apply; cubevol then
 * only touches backlight on hotkeys, so our value sticks. */
int settings_bl_reassert = 0;
int settings_audio_mute_reassert = 0;
int settings_filter_idx = 1; /* forced bilinear (option removed from menu) */
int settings_filter_idx_on_enter = 0; /* snapshot for restart-on-change */
int settings_quick_resume =
    0; /* boot straight into last game: 0=off, 1=on. Settings key stays
          "auto_resume" for upgrade compat. */
int settings_custom_aspect_ratios =
    0; /* expose forced aspect-ratio choices in PicoArch */
int settings_autosave_autoload =
    1; /* auto-save on pause/quit + auto-load on any game launch (boot or manual
          pick): 0=off, 1=on */
int settings_anim = 1;        /* UI animations: 0=off, 1=on */
int settings_menu_sounds = 0; /* short navigation tick: 0=off, 1=on */
int settings_style = STYLE_VERTICAL;
int settings_center_text = 0; /* center labels in the vertical system list */
int settings_friendly_names =
    0;                         /* expand system folder codes in either style */
int settings_gamelist_dir = 1; /* gamelist.xml systems: 1 = gamelist + files on
                                  disk, 0 = gamelist only */
int settings_list_info =
    1; /* description/info under the box art in the browser */
int settings_hide_empty = 1; /* hide rom folders with no games: 0=off, 1=on */
int settings_hide_extensions =
    1; /* hide file extensions in the browser: 0=off, 1=on */
int settings_backgrounds =
    1; /* show per-system background images: 0=off (solid theme bg), 1=on */
int settings_background_dim =
    15; /* darken background artwork: 0=unchanged, 100=black */
int settings_file_cache =
    1; /* cache folder listings (mtime-keyed) for fast nav: 0=off, 1=on */
int settings_battery_color =
    0; /* "Nel Battery Mode": solid color light by level instead of fill bar */
int settings_language;
int frogui_battery_color_mode(void) { return settings_battery_color; }
int settings_game_switcher = 1; /* recents as box-art carousel: 0=off, 1=on */
int settings_load_recents =
    0; /* start FrogUI in the recents view: 0=off, 1=on */
int settings_rom_source = ROM_SOURCE_SD;
const char *rom_source_names[ROM_SOURCE_COUNT] = {"SD", "OTG"};
int settings_disable_sleep =
    1; /* live-patch cubevol to disable power sleep: 0=off, 1=on. zhijack reads
          this at boot; applies after restart. Default ON: R36SX/SF3500-class
          sleep isn't supported by TreeFrogUI, so ship with it disabled and
          point users at Quick Resume instead. */
int settings_volume =
    100; /* global output volume 0..100 → cubegm/sndgain.txt */
static const char *filter_names[] = {"nearest", "bilinear"};
static const char *onoff_names[] = {"off", "on"};
const char *style_names[STYLE_COUNT] = {"Vertical", "Horizontal",
                                        "System View"};
static const char *style_keys[STYLE_COUNT] = {"vertical", "horizontal",
                                              "system"};
const char *treefrogui_version(void) {
  static char version[32];
  static int loaded;
  if (!loaded) {
    FILE *f = fopen("/mnt/sdcard/cubegm/version.txt", "r");
    if (f) {
      if (!fgets(version, sizeof version, f))
        version[0] = '\0';
      fclose(f);
      version[strcspn(version, "\r\n")] = '\0';
    }
    if (!version[0])
      snprintf(version, sizeof version, "unknown");
    loaded = 1;
  }
  return version;
}
static void mkdir_p(const char *path) {
  char cmd[300];
  snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
  system(cmd);
}
static void settings_write_volume(void) {
  /* The shared system volume lives in cubevol's persistentmem slot (the
   * physical volume buttons' value); writing it also mirrors to the I2SO
   * hardware path and legacy sndgain.txt for standalone frontends.
   * Called on confirmed adjustments (menu exit / boot apply) AND as the
   * debounced commit of the live slider path (see settings_preview_row):
   * cube_pmem_volume_write itself skips the EEPROM write when the level
   * is unchanged, so repeated calls at the same value are free. */
  if (settings_volume < 0)
    settings_volume = 0;
  if (settings_volume > 100)
    settings_volume = 100;
  if (cube_pmem_volume_write(settings_volume) < 0)
    dbg("volume persist failed (pmem); hw level applied, stored value "
        "unchanged");
}
void settings_apply(void) {
  extern const int theme_count;
  if (settings_theme_idx < 0 || settings_theme_idx >= theme_count)
    settings_theme_idx = 0;
  if (settings_style < 0 || settings_style >= STYLE_COUNT)
    settings_style = STYLE_VERTICAL;
  if (settings_font_idx < 0 || settings_font_idx >= font_count)
    settings_font_idx = 0;
  if (settings_font_size < 18)
    settings_font_size = 18;
  if (settings_font_size > 26)
    settings_font_size = 26;
  if (settings_brightness < 0)
    settings_brightness = 0;
  if (settings_brightness > 100)
    settings_brightness = 100;
  if (settings_background_dim < 0)
    settings_background_dim = 0;
  if (settings_background_dim > 100)
    settings_background_dim = 100;
  theme_apply(settings_theme_idx);
  theme_sync_artwork_pack();
  if (font_count > 0)
    font_load_file(font_files[settings_font_idx]);
  font_set_size(settings_font_size);
  font_sync_language_fallback();
  cube_set_backlight(settings_brightness);
  /* Keep cubevol's persistentmem value in sync so its delayed startup apply
   * shows the right brightness instead of flashing its stored default. */
  cube_pmem_backlight_sync(settings_brightness);
  banner_set_anim(settings_anim);
  banner_set_dim(settings_background_dim);
  /* Global output volume: there's no system mixer on this hardware, so every
   * frontend (picoarch, pcsx4all, lgpt) reads this percent from sndgain.txt and
   * scales its own audio. FrogUI is the single place to set it. */
  if (settings_volume < 0)
    settings_volume = 0;
  if (settings_volume > 100)
    settings_volume = 100;
  settings_write_volume();
  /* FrogUI has no continuous audio unless menu sounds are explicitly enabled.
     Zeroed samples leave the DAC/amp live; mute its real I2SO output instead.
   */
  cube_set_i2so_output_muted(settings_menu_sounds ? 0 : 1);
}
long long volume_last_commit_ms = 0;
int volume_committed_level = -1;
void settings_volume_commit_live(void) {
  long long now = monotonic_ms();
  if (settings_volume == volume_committed_level)
    return; /* nothing new to persist */
  if (now - volume_last_commit_ms < VOLUME_COMMIT_MS &&
      volume_committed_level >= 0)
    return; /* inside the debounce window; next step (or the final
               commit below) will persist it */
  volume_last_commit_ms = now;
  volume_committed_level = settings_volume;
  settings_write_volume();
}
void settings_volume_commit_final(void) {
  volume_committed_level = -1; /* force a real commit check on next slide */
  volume_last_commit_ms = monotonic_ms();
}
void settings_load_file(void) {
  FILE *f = fopen(SETTINGS_FILE, "r");
  if (!f)
    return;
  char line[256];
  extern const int theme_count;
  extern const Theme themes[];
  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = '\0';
    char *val = eq + 1;
    char *nl = strchr(val, '\n');
    if (nl)
      *nl = '\0';
    char *cr = strchr(val, '\r');
    if (cr)
      *cr = '\0';
    if (strcmp(line, "theme") == 0) {
      /* v1.0.11 briefly shipped Horizontal as a colour theme. Preserve
       * that layout choice while migrating back to the default colours. */
      if (strcmp(val, "Horizontal") == 0) {
        settings_theme_idx = 0;
        settings_style = STYLE_HORIZONTAL;
        settings_friendly_names = 1;
      } else {
        for (int i = 0; i < theme_count; i++)
          if (strcmp(themes[i].name, val) == 0) {
            settings_theme_idx = i;
            break;
          }
      }
    } else if (strcmp(line, "font") == 0) {
      for (int i = 0; i < font_count; i++)
        if (strcasecmp(font_files[i], val) == 0 ||
            strcasecmp(font_disp[i], val) == 0) {
          settings_font_idx = i;
          break;
        }
    } else if (strcmp(line, "font_size") == 0) {
      settings_font_size = atoi(val);
    } else if (strcmp(line, "language") == 0) {
      for (int i = 0; i < i18n_language_count(); i++)
        if (strcasecmp(val, i18n_language_code_at(i)) == 0) {
          settings_language = i;
          break;
        }
    } else if (strcmp(line, "wallpaper") == 0) {
      settings_wallpaper_idx = 0; /* default None */
      for (int i = 1; i < wallpaper_count; i++)
        if (strcasecmp(wallpaper_files[i], val) == 0) {
          settings_wallpaper_idx = i;
          break;
        }
    } else if (strcmp(line, "wallpaper_fit") == 0) {
      for (int i = 0; i < WALL_FIT_N; i++)
        if (strcasecmp(wallpaper_fit_names[i], val) == 0) {
          settings_wallpaper_fit = i;
          break;
        }
    } else if (strcmp(line, "theme_pack") == 0) {
      for (int i = 0; i < theme_pack_count; i++)
        if (strcasecmp(theme_pack_files[i], val) == 0 ||
            strcasecmp(theme_pack_disp[i], val) == 0) {
          settings_theme_pack_idx = i;
          break;
        }
    } else if (strcmp(line, "icon_pack") == 0) {
      settings_icon_pack_idx = 0;
      for (int i = 1; i < icon_pack_count; i++)
        if (strcasecmp(icon_pack_files[i], val) == 0 ||
            strcasecmp(icon_pack_disp[i], val) == 0) {
          settings_icon_pack_idx = i;
          break;
        }
    } else if (strcmp(line, "brightness") == 0) {
      settings_brightness = atoi(val);
    } else if (strcmp(line, "volume") == 0) {
      settings_volume = atoi(val);
    } else if (strcmp(line, "filter") == 0) {
      for (int i = 0; i < FILTER_COUNT; i++)
        if (strcmp(filter_names[i], val) == 0) {
          settings_filter_idx = i;
          break;
        }
    } else if (strcmp(line, "animations") == 0) {
      settings_anim = (strcmp(val, "off") == 0) ? 0 : 1;
    } else if (strcmp(line, "menu_sounds") == 0) {
      settings_menu_sounds = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "style") == 0) {
      for (int i = 0; i < STYLE_COUNT; i++)
        if (strcasecmp(val, style_keys[i]) == 0) {
          settings_style = i;
          break;
        }
    } else if (strcmp(line, "center_text") == 0) {
      settings_center_text = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "horizontal_layout") == 0) {
      /* Migrate the short-lived toggle form without changing how that
       * user's carousel labels looked. */
      settings_style =
          (strcmp(val, "on") == 0) ? STYLE_HORIZONTAL : STYLE_VERTICAL;
      if (settings_style == STYLE_HORIZONTAL)
        settings_friendly_names = 1;
    } else if (strcmp(line, "friendly_system_names") == 0) {
      settings_friendly_names = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "auto_resume") == 0) {
      settings_quick_resume = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "autosave_autoload") == 0) {
      settings_autosave_autoload = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "custom_aspect_ratios") == 0) {
      settings_custom_aspect_ratios = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "gamelist_dir") == 0) {
      settings_gamelist_dir = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "list_info") == 0) {
      settings_list_info = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "hide_empty") == 0) {
      settings_hide_empty = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "hide_extensions") == 0) {
      settings_hide_extensions = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "backgrounds") == 0) {
      settings_backgrounds = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "background_dim") == 0) {
      settings_background_dim = atoi(val);
    } else if (strcmp(line, "file_cache") == 0) {
      settings_file_cache = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "folder_cache") == 0) {
      /* Legacy alias: preserve the user's old cache preference. */
      settings_file_cache = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "battery_color") == 0) {
      settings_battery_color = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "disable_sleep") == 0) {
      settings_disable_sleep = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "game_switcher") == 0) {
      settings_game_switcher = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "load_recents") == 0) {
      settings_load_recents = (strcmp(val, "on") == 0) ? 1 : 0;
    } else if (strcmp(line, "rom_source") == 0) {
      settings_rom_source =
          (strcasecmp(val, "otg") == 0) ? ROM_SOURCE_OTG : ROM_SOURCE_SD;
    }
  }
  fclose(f);
  /* The shared system volume (cubevol persistentmem - what the PHYSICAL
   * volume buttons last set) always wins over our saved copy: the user
   * may have changed it with the buttons since the last Settings visit. */
  int shared_vol = cube_pmem_volume_read();
  if (shared_vol >= 0 && shared_vol <= 100)
    settings_volume = shared_vol;
}
/* The rootfs mdev hook mounts a connected OTG drive at /media/hdd. Check both
 * the mount table and its roms directory so a stale mountpoint never moves the
 * library away from the internal SD. */
bool otg_roms_available(void) {
  FILE *mounts = fopen("/proc/mounts", "r");
  char device[256], target[256], fstype[64], options[256];
  int mounted = 0;
  struct stat st;
  if (!mounts)
    return false;
  while (fscanf(mounts, "%255s %255s %63s %255s %*d %*d", device, target,
                fstype, options) == 4) {
    if (strcmp(target, OTG_MOUNT_PATH) == 0) {
      mounted = 1;
      break;
    }
  }
  fclose(mounts);
  return mounted && stat(OTG_ROMS_PATH, &st) == 0 && S_ISDIR(st.st_mode);
}
/* FROGUI_ROMS_PATH is retained for development. Normal use selects the SD or
 * a connected OTG drive through Settings; a missing OTG drive safely falls
 * back to the internal SD and is never created or modified. */
void resolve_roms_root(void) {
  const char *override = getenv("FROGUI_ROMS_PATH");
  struct stat st;
  if (override && *override && strlen(override) < sizeof(g_roms_path) &&
      stat(override, &st) == 0 && S_ISDIR(st.st_mode)) {
    strncpy(g_roms_path, override, sizeof(g_roms_path) - 1);
    g_roms_path[sizeof(g_roms_path) - 1] = '\0';
    while (strlen(g_roms_path) > 1 &&
           g_roms_path[strlen(g_roms_path) - 1] == '/')
      g_roms_path[strlen(g_roms_path) - 1] = '\0';
    return;
  }
  if (settings_rom_source == ROM_SOURCE_OTG && otg_roms_available()) {
    strncpy(g_roms_path, OTG_ROMS_PATH, sizeof(g_roms_path) - 1);
    g_roms_path[sizeof(g_roms_path) - 1] = '\0';
    return;
  }
  strcpy(g_roms_path, ROMS_PATH_DEFAULT);
}
void settings_save_file(void) {
  extern const Theme themes[];
  mkdir_p(SETTINGS_DIR);
  FILE *f = fopen(SETTINGS_FILE, "w");
  if (!f) {
    dbg("settings save: fopen failed");
    return;
  }
  fprintf(f, "theme=%s\n", themes[settings_theme_idx].name);
  fprintf(f, "font=%s\n", font_count > 0 ? font_files[settings_font_idx] : "");
  fprintf(f, "font_size=%d\n", settings_font_size);
  fprintf(f, "language=%s\n", i18n_current_language());
  fprintf(
      f, "wallpaper=%s\n",
      (settings_wallpaper_idx > 0 && settings_wallpaper_idx < wallpaper_count)
          ? wallpaper_files[settings_wallpaper_idx]
          : "none");
  fprintf(f, "wallpaper_fit=%s\n", wallpaper_fit_names[settings_wallpaper_fit]);
  fprintf(f, "theme_pack=%s\n",
          settings_theme_pack_idx > 0
              ? theme_pack_files[settings_theme_pack_idx]
              : "default");
  fprintf(f, "icon_pack=%s\n",
          settings_icon_pack_idx > 0 ? icon_pack_files[settings_icon_pack_idx]
                                     : "default");
  fprintf(f, "brightness=%d\n", settings_brightness);
  fprintf(f, "volume=%d\n", settings_volume);
  fprintf(f, "filter=%s\n", filter_names[settings_filter_idx]);
  fprintf(f, "auto_resume=%s\n", onoff_names[settings_quick_resume]);
  fprintf(f, "autosave_autoload=%s\n", onoff_names[settings_autosave_autoload]);
  fprintf(f, "custom_aspect_ratios=%s\n",
          onoff_names[settings_custom_aspect_ratios]);
  fprintf(f, "animations=%s\n", onoff_names[settings_anim]);
  fprintf(f, "menu_sounds=%s\n", onoff_names[settings_menu_sounds]);
  fprintf(f, "style=%s\n", style_keys[settings_style]);
  fprintf(f, "center_text=%s\n", onoff_names[settings_center_text]);
  fprintf(f, "friendly_system_names=%s\n",
          onoff_names[settings_friendly_names]);
  fprintf(f, "gamelist_dir=%s\n", onoff_names[settings_gamelist_dir]);
  fprintf(f, "list_info=%s\n", onoff_names[settings_list_info]);
  fprintf(f, "hide_empty=%s\n", onoff_names[settings_hide_empty]);
  fprintf(f, "hide_extensions=%s\n", onoff_names[settings_hide_extensions]);
  fprintf(f, "backgrounds=%s\n", onoff_names[settings_backgrounds]);
  fprintf(f, "background_dim=%d\n", settings_background_dim);
  fprintf(f, "file_cache=%s\n", onoff_names[settings_file_cache]);
  fprintf(f, "battery_color=%s\n", onoff_names[settings_battery_color]);
  fprintf(f, "game_switcher=%s\n", onoff_names[settings_game_switcher]);
  fprintf(f, "load_recents=%s\n", onoff_names[settings_load_recents]);
  fprintf(f, "rom_source=%s\n", rom_source_names[settings_rom_source]);
  fprintf(f, "disable_sleep=%s\n", onoff_names[settings_disable_sleep]);
  fprintf(f, "sections=%d%d%d%d%d\n", settings_section_open[0] ? 1 : 0,
          settings_section_open[1] ? 1 : 0, settings_section_open[2] ? 1 : 0,
          settings_section_open[3] ? 1 : 0, settings_section_open[4] ? 1 : 0);
  fflush(f);
  fsync(fileno(f));
  fclose(f);
  cube_pmem_backlight_sync(settings_brightness);
  settings_write_volume();
  sync(); /* SD-card flush */
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "settings save: filter=%s idx=%d",
             filter_names[settings_filter_idx], settings_filter_idx);
    dbg(buf);
  }
}
