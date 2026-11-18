/*
 * frogui_libretro.c - FrogUI as a libretro core for SF3000
 *
 * Picoarch loads this core and handles ALL display/input/SDL init.
 * FrogUI renders its menu into a RGB565 buffer and passes it to video_cb.
 * When user picks a game, write /tmp/frogui_launch.txt and signal SHUTDOWN.
 * icube reads that file and launches picoarch with the real game core.
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "backlight.h"
#include "banner.h"
#include "common/i18n.h"
#include "core_map.h"
#include "battery.h"
#include "debug.h"
#include "core_override.h"
#include "ext_filter.h"
#include "favorites.h"
#include "font.h"
#include "frogui_config.h"
#include "input.h"
#include "libretro.h"
#include "paths.h"
#include "recent_games.h"
#include "render.h"
#include "theme.h"
#include "app.h"







/* Libretro callbacks */
static retro_video_refresh_t video_cb = NULL;
retro_environment_t environ_cb = NULL;
retro_audio_sample_t audio_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_input_poll_t input_poll_cb = NULL;
static retro_input_state_t input_state_cb = NULL;







/* --- Input (via input.c / cubevol shmem) --- */














/* Filter option removed from the menu — always bilinear (HW path). */







































/* ---------------- OnionOS-style game switcher (recents as box-art carousel)
 * ---- Art = the newest gameplay screenshot picoarch wrote (.scr.bmp or
 * .st<N>.bmp); if no capture exists, fall back to box art/title artwork.
 * Toggled by settings_game_switcher. */
























/* ----------------------------- Search (X button) -----------------------------
 */







































static void handle_input(void) {
  input_update();

  /* A modal action must not fall through to the browser while its physical
   * button is still held.  In particular, B previously cancelled the modal
   * and then immediately performed a second, unrelated browser action. */
  if (usb_mode_wait_for_release) {
    if (input_get_raw_state() == 0)
      usb_mode_wait_for_release = false;
    else
      return;
  }

  /* USB mode gets its own confirmation page.  Keeping this modal prevents
   * the second A press from falling through into the browser (which used to
   * look like a UI restart) and gives B a reliable cancel path. */
  if (usb_mode_confirm_active || usb_mode_initiated_active) {
    uint32_t raw = input_get_raw_state();
    bool b_edge = usb_mode_raw_edge(FROG_BTN_B, raw);
    bool a_edge = usb_mode_raw_edge(FROG_BTN_A, raw);
    usb_mode_prev_raw = raw;

    /* B is identical in both phases.  The modal consumes every other
     * button, so browser navigation cannot accidentally close it. */
    if (b_edge) {
      usb_mode_confirm_active = false;
      usb_mode_initiated_active = false;
      usb_mode_wait_for_release = true;
      ui_toast_frames = 0;
      return;
    }
    if (usb_mode_initiated_active) {
      if (monotonic_ms() - usb_mode_initiated_at_ms >= 750) {
        usb_mode_initiated_active = false;
        request_builtin_launch(USB_MODE_BIN);
      }
      return;
    }
    if (a_edge) {
      usb_mode_confirm_active = false;
      usb_mode_initiated_active = true;
      usb_mode_initiated_at_ms = monotonic_ms();
    }
    return;
  }

  /* Search keyboard overlay */
  if (search_kbd_active) {
    handle_search_kbd();
    return;
  }

  /* Extension-entry keyboard overlay (SELECT picker ??? Add extension...) */
  if (extf_kbd_active) {
    handle_extf_kbd();
    return;
  }

  /* Core picker overlay: choose an override core for the current ROM/folder */
  if (core_picker_active) {
    handle_core_picker();
    return;
  }

  /* Remap wizard: detect raw rising edge on any bit; B = skip this step */
  if (remap_wizard_active) {
    handle_remap_wizard();
    return;
  }

  /* Top-level tabs use shoulders only. D-pad Left/Right belongs to the
   * current view (including Vertical) and must never change tabs. */
  {
    int tab = main_tab_active();
    bool at_tabs = settings_menu_active || viewing_recents || viewing_apps ||
                   apps_browsing ||
                   (!viewing_favourites && !viewing_search &&
                    strcmp(current_path, ROMS_PATH) == 0);
    bool prev = input_was_pressed(FROG_BTN_L1);
    bool next = input_was_pressed(FROG_BTN_R1);
    if (at_tabs && prev && tab > MAIN_TAB_RECENTS) {
      switch_main_tab(tab - 1);
      return;
    }
    if (at_tabs && next && tab < MAIN_TAB_SETTINGS) {
      switch_main_tab(tab + 1);
      return;
    }
  }

  if (settings_menu_active) {
    handle_settings_menu();
    return;
  }

  /* X: open search. Scope = current folder (ROMS root → search everything). */
  if (input_was_pressed(FROG_BTN_X) && !viewing_recents &&
      !viewing_favourites && !viewing_search) {
    search_query[0] = '\0';
    search_kbd_r = 0;
    search_kbd_c = 0;
    strncpy(search_scope, current_path, sizeof(search_scope) - 1);
    search_scope[sizeof(search_scope) - 1] = '\0';
    search_kbd_active = true;
    goto input_done;
  }

  /* SELECT: open core picker. On a ROM file → per-game override; on a system
   * folder → per-folder override. Not available in recents/favourites views. */
  if (input_was_pressed(FROG_BTN_SELECT) && !viewing_recents &&
      !viewing_favourites && !viewing_search && selected_index < entry_count &&
      strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
      strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) != 0) {
    const char *cur;
    if (entries[selected_index].is_dir) {
      snprintf(core_picker_key, sizeof(core_picker_key), "%s/%s", current_path,
               entries[selected_index].name);
      snprintf(core_picker_title, sizeof(core_picker_title), "Folder: %s",
               entries[selected_index].name);
      cur = core_override_lookup(NULL, core_picker_key);
    } else {
      snprintf(core_picker_key, sizeof(core_picker_key), "%s/%s", current_path,
               entries[selected_index].name);
      snprintf(core_picker_title, sizeof(core_picker_title), "Game: %s",
               entries[selected_index].name);
      cur = core_override_lookup(core_picker_key, NULL);
    }
    /* Cursor lands on the active core's row (or the first core row when
     * the section is collapsed). */
    core_picker_idx = core_choice_index_for_path(cur);
    core_picker_current = core_picker_idx; /* the active core, marked "* " */
    /* Keep the SELECT menu compact on entry.  Cores are the common
     * action; extension filtering remains available under its collapsible
     * section when needed. */
    extf_cores_open = true;
    extf_exts_open = false;
    core_picker_idx += 1; /* +1: ">> Cores" header row */
    core_picker_scroll = 0;
    if (core_picker_idx >= PICKER_ROWS)
      core_picker_scroll = core_picker_idx - PICKER_ROWS + 1;
    /* Extension whitelist block (PS1 family) sits ABOVE the core list.
     * Show it when SELECTing a filtered system folder itself, or when the
     * picker is opened on a ROM file inside one. */
    extf_in_picker = false;
    extf_inside = false;
    if (entries[selected_index].is_dir) {
      /* Every browsable folder gets the same extension-filter editor;
       * an empty list simply starts disabled and does not hide files. */
      {
        extf_in_picker = true;
        strncpy(extf_folder, entries[selected_index].name,
                sizeof(extf_folder) - 1);
        extf_folder[sizeof(extf_folder) - 1] = '\0';
        snprintf(extf_path, sizeof(extf_path), "%s/%s", current_path,
                 entries[selected_index].name);
      }
    } else {
      const char *cf = get_filter_folder(current_path);
      if (cf) {
        extf_in_picker = true;
        extf_inside = true;
        strncpy(extf_folder, cf, sizeof(extf_folder) - 1);
        extf_folder[sizeof(extf_folder) - 1] = '\0';
        strncpy(extf_path, current_path, sizeof(extf_path) - 1);
        extf_path[sizeof(extf_path) - 1] = '\0';
      }
    }
    extf_dirty = false;
    core_picker_active = true;
    return;
  }

  /* Y: Onion-style detailed/fullscreen toggle in GameSwitcher. Elsewhere it
   * keeps the existing favourite action. */
  if (input_was_pressed(FROG_BTN_Y) && viewing_recents &&
      settings_game_switcher) {
    game_switcher_fullscreen = !game_switcher_fullscreen;
    goto input_done;
  }
  if (input_was_pressed(FROG_BTN_Y) && selected_index < entry_count) {
    if (viewing_favourites) {
      /* Remove selected favourite */
      favorites_remove_by_index(selected_index);
      ui_toast_show("Removed from Favourites");
      /* Refresh favourites view */
      const FavoriteGame *fl = favorites_get_list();
      int fc = favorites_get_count();
      entry_count = 0;
      for (int i = 0; i < fc; i++) {
        strncpy(entries[entry_count].name, fl[i].game_name, 255);
        entries[entry_count].name[255] = '\0';
        entries[entry_count].is_dir = 0;
        entry_count++;
      }
      if (selected_index >= entry_count && selected_index > 0)
        selected_index = entry_count - 1;
      if (entry_count == 0) {
        /* No more favourites — go back to root */
        viewing_favourites = false;
        scan_directory(ROMS_PATH);
        strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
      }
    } else if (!viewing_recents && !entries[selected_index].is_dir &&
               strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
               strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
               strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) !=
                   0) {
      /* Toggle favourite for current ROM */
      const char *folder = get_console_folder(current_path);
      const char *core = get_core_for_folder(folder);
      if (!core)
        core = get_core_for_extension(entries[selected_index].name);
      char rom_path[MAX_PATH_LEN];
      snprintf(rom_path, sizeof(rom_path), "%s/%s", current_path,
               entries[selected_index].name);
      char game_name[256];
      strncpy(game_name, entries[selected_index].name, sizeof(game_name) - 1);
      game_name[sizeof(game_name) - 1] = '\0';
      char *dot = strrchr(game_name, '.');
      if (dot)
        *dot = '\0';
      if (core) {
        int was_favourite = favorites_is_favorited(core, game_name);
        favorites_toggle(core, game_name, rom_path);
        ui_toast_show(was_favourite ? "Removed from Favourites"
                                    : "Added to Favourites");
      }
    }
  }

  if (input_was_pressed(FROG_BTN_A) && selected_index < entry_count) {
    if (viewing_search) {
      search_launch(selected_index);
    } else if (viewing_favourites) {
      /* Launch from favourites list */
      const FavoriteGame *fl = favorites_get_list();
      int fc = favorites_get_count();
      if (selected_index < fc)
        launch_by_path(fl[selected_index].full_path);
    } else if (viewing_activity) {
      if (selected_index < activity_count)
        launch_by_path(activity_paths[selected_index]);
    } else if (viewing_recents) {
      /* Launch from recent games list */
      const RecentGame *rg = recent_games_get_list();
      int rc = recent_games_get_count();
      if (selected_index < rc)
        launch_by_path(rg[selected_index].full_path);
    } else if (viewing_apps) {
      /* Apps entries are virtual aliases for the on-card media folders. */
      char app_path[MAX_PATH_LEN];
      int app_index = -1;
      if (selected_index < entry_count) {
        for (int ai = 0; ai < app_def_count();
             ai++)
          if (strcmp(entries[selected_index].name, app_defs[ai].key) == 0) {
            app_index = ai;
            break;
          }
      }
      if (app_index >= 0 && !strcmp(app_defs[app_index].key, "activity")) {
        ui_transition_start(1);
        enter_activity_view();
      } else if (app_index >= 0 &&
                 !strcmp(app_defs[app_index].key, "shutdown")) {
        request_builtin_launch(SHUTDOWN_BIN);
      } else if (app_index >= 0 &&
                 !strcmp(app_defs[app_index].key, "usbmode")) {
        if (access(USB_MODE_BIN, X_OK) != 0)
          ui_toast_show("USB mode is unavailable");
        else {
          ui_toast_frames = 0;
          usb_mode_confirm_active = true;
          /* Do not treat the A which opened the app as an A which
           * confirms the modal. */
          usb_mode_prev_raw = input_get_raw_state();
        }
      } else if (app_index >= 0 && app_defs[app_index].bin &&
                 access(app_defs[app_index].bin, X_OK) == 0) {
        if (!strcmp(app_defs[app_index].bin, FROGSHELL_CORE))
          request_game_launch(FROGSHELL_CORE, FROGSHELL_CORE);
        else
          request_standalone_launch(app_defs[app_index].bin, ROMS_PATH);
      } else if (app_index >= 0 &&
                 app_folder_path(app_index, app_path, sizeof app_path)) {
        ui_transition_start(1);
        strncpy(current_path, app_path, MAX_PATH_LEN - 1);
        current_path[MAX_PATH_LEN - 1] = '\0';
        strncpy(apps_root_path, app_path, MAX_PATH_LEN - 1);
        apps_root_path[MAX_PATH_LEN - 1] = '\0';
        viewing_apps = false;
        scan_directory(current_path);
        apps_browsing = true;
      }
    } else if (strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) ==
               0) {
      /* Enter favourites view */
      ui_transition_start(1);
      const FavoriteGame *fl = favorites_get_list();
      int fc = favorites_get_count();
      while (fc > entry_capacity) {
        entry_capacity =
            entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
        entries = realloc(entries, entry_capacity * sizeof(DirEntry));
        if (!entries)
          goto input_done;
      }
      entry_count = 0;
      for (int i = 0; i < fc; i++) {
        strncpy(entries[entry_count].name, fl[i].game_name, 255);
        entries[entry_count].name[255] = '\0';
        entries[entry_count].is_dir = 0;
        entry_count++;
      }
      viewing_favourites = true;
      selected_index = 0;
      scroll_offset = 0;
    } else if (strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) == 0) {
      ui_transition_start(1);
      enter_recents_view();
    } else if (entries[selected_index].is_dir) {
      ui_transition_start(1);
      char new_path[MAX_PATH_LEN];
      snprintf(new_path, sizeof(new_path), "%s/%s", current_path,
               entries[selected_index].name);
      strncpy(current_path, new_path, MAX_PATH_LEN - 1);
      current_path[MAX_PATH_LEN - 1] = '\0';
      scan_directory(current_path);
    } else {
      if (strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) == 0) {
        settings_menu_active = true;
        settings_menu_idx = 0;
        settings_filter_idx_on_enter = settings_filter_idx;
      } else {
        char rom_path[MAX_PATH_LEN];
        snprintf(rom_path, sizeof(rom_path), "%s/%s", current_path,
                 entries[selected_index].name);
        launch_by_path(rom_path);
      }
    }
  }

  if (input_was_pressed(FROG_BTN_B)) {
    if (viewing_search) {
      /* back to the keyboard to refine the query */
      viewing_search = false;
      search_kbd_active = true;
    } else if (viewing_activity) {
      ui_transition_start(-1);
      scan_apps_tab();
      return;
    } else if (viewing_recents) {
      switch_main_tab(MAIN_TAB_GAMES);
      return;
    } else if (apps_browsing && strcmp(current_path, apps_root_path) == 0) {
      /* Return from a media folder to the Apps tab, not the Games root. */
      ui_transition_start(-1);
      strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
      current_path[MAX_PATH_LEN - 1] = '\0';
      scan_apps_tab();
      return;
    } else if (viewing_favourites) {
      ui_transition_start(-1);
      viewing_favourites = false;
      scan_directory(ROMS_PATH);
      strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
    } else if (strcmp(current_path, ROMS_PATH) != 0) {
      ui_transition_start(-1);
      char *slash = strrchr(current_path, '/');
      if (slash) {
        /* scan_directory resets selection for a fresh listing. Keep
         * the folder name so returning from a system restores the row
         * we entered instead of jumping to the first item. */
        char child[MAX_PATH_LEN];
        strncpy(child, slash + 1, sizeof(child) - 1);
        child[sizeof(child) - 1] = '\0';
        *slash = '\0';
        scan_directory(current_path);
        for (int i = 0; i < entry_count; i++) {
          if (strcmp(entries[i].name, child) == 0) {
            selected_index = i;
            scroll_offset =
                (i >= VISIBLE_ENTRIES) ? i - VISIBLE_ENTRIES + 1 : 0;
            break;
          }
        }
      }
    }
  }

  if (horizontal_system_view()) {
    scroll_offset = 0;
    if (input_repeat(FROG_BTN_LEFT) && entry_count > 0) {
      selected_index = (selected_index + entry_count - 1) % entry_count;
      system_carousel_start(-1);
    }
    if (input_repeat(FROG_BTN_RIGHT) && entry_count > 0) {
      selected_index = (selected_index + 1) % entry_count;
      system_carousel_start(1);
    }
  } else if (icon_system_view()) {
    enum { GRID_COLS = 4 };
    scroll_offset = 0;
    if (input_repeat(FROG_BTN_LEFT) && entry_count > 0)
      selected_index = (selected_index + entry_count - 1) % entry_count;
    if (input_repeat(FROG_BTN_RIGHT) && entry_count > 0)
      selected_index = (selected_index + 1) % entry_count;
    if (input_repeat(FROG_BTN_UP) && entry_count > 0) {
      int next = selected_index - GRID_COLS;
      if (next < 0) {
        int col = selected_index % GRID_COLS;
        int last_row = (entry_count - 1) / GRID_COLS;
        next = last_row * GRID_COLS + col;
        if (next >= entry_count)
          next = entry_count - 1;
      }
      selected_index = next;
    }
    if (input_repeat(FROG_BTN_DOWN) && entry_count > 0) {
      int next = selected_index + GRID_COLS;
      if (next >= entry_count)
        next = selected_index % GRID_COLS;
      if (next >= entry_count)
        next = entry_count - 1;
      selected_index = next;
    }
  } else {
    if (input_repeat(FROG_BTN_UP) && selected_index > 0) {
      selected_index--;
      if (selected_index < scroll_offset)
        scroll_offset = selected_index;
    }
    if (input_repeat(FROG_BTN_DOWN) && selected_index < entry_count - 1) {
      selected_index++;
      if (selected_index >= scroll_offset + VISIBLE_ENTRIES)
        scroll_offset = selected_index - VISIBLE_ENTRIES + 1;
    }
    /* In the game switcher (one game on screen at a time), Left/Right step
     * one game like Up/Down — no page jumping. */
    bool switcher = viewing_recents && settings_game_switcher;
    if (input_repeat(FROG_BTN_LEFT)) {
      if (switcher) {
        if (selected_index > 0)
          selected_index--;
      } else {
        selected_index = (selected_index >= VISIBLE_ENTRIES)
                             ? selected_index - VISIBLE_ENTRIES
                             : 0;
        if (selected_index < scroll_offset)
          scroll_offset = selected_index;
      }
    }
    if (input_repeat(FROG_BTN_RIGHT)) {
      if (switcher) {
        if (selected_index < entry_count - 1)
          selected_index++;
      } else {
        selected_index = (selected_index + VISIBLE_ENTRIES < entry_count)
                             ? selected_index + VISIBLE_ENTRIES
                             : entry_count - 1;
        if (selected_index >= scroll_offset + VISIBLE_ENTRIES)
          scroll_offset = selected_index - VISIBLE_ENTRIES + 1;
      }
    }
  }

input_done:;
}

/* ---- libretro API ---- */

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
  memset(info, 0, sizeof(*info));
  info->library_name = "TreeFrogUI";
  info->library_version = "1.0-sf3000";
  info->valid_extensions = "";
  info->need_fullpath = false;
  info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
  memset(info, 0, sizeof(*info));
  info->geometry.base_width = SCREEN_WIDTH;
  info->geometry.base_height = SCREEN_HEIGHT;
  info->geometry.max_width = SCREEN_WIDTH;
  info->geometry.max_height = SCREEN_HEIGHT;
  info->geometry.aspect_ratio = (float)SCREEN_WIDTH / SCREEN_HEIGHT;
  info->timing.fps = 60.0;
  info->timing.sample_rate = (double)SAMPLE_RATE;
}

void retro_set_environment(retro_environment_t cb) {
  environ_cb = cb;
  bool no_game = true;
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
  cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
  audio_batch_cb = cb;
}
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void) {
  dbg("retro_init start");
  /* Runtime panel geometry from picoarch (device-detected). Must run before
   * font_init / any layout use. Falls back to render.c defaults if env unset.
   */
  {
    const char *ew = getenv("TF_PANEL_W");
    const char *eh = getenv("TF_PANEL_H");
    const char *es = getenv("TF_UI_SCALE");
    int w = ew ? atoi(ew) : 0;
    int h = eh ? atoi(eh) : 0;
    int s = es ? atoi(es) : 0;
    render_set_geometry(w, h, s);
    dbg("geometry set");
  }
  input_init();
  input_load_remap(KEYMAP_FILE);
  i18n_init("en_US");
  i18n_scan_languages();
  font_init();
  dbg("font_init done");
  font_scan();
  dbg("font_scan done");
  wallpaper_scan();
  dbg("wallpaper_scan done");
  theme_pack_scan();
  dbg("theme_pack_scan done");
  icon_pack_scan();
  dbg("icon_pack_scan done");
  theme_init();
  dbg("theme_init done");
  settings_load_file();
  i18n_init(i18n_language_code_at(settings_language));
  system_labels_refresh();
  settings_sections_load(); /* restore the user's collapsed sections */
  /* A language change requests a normal launcher restart. Consume the flag
   * once and reopen the same Settings row so the user can continue without
   * hunting through the menu. */
  if (access(LANGUAGE_RESTART_FLAG, F_OK) == 0) {
    unlink(LANGUAGE_RESTART_FLAG);
    settings_menu_active = true;
    settings_section_open[1] = true; /* General contains Language. */
    for (int i = 0; i < SETTINGS_ROW_N; i++)
      if (settings_rows[i].type == RT_LANGUAGE) {
        settings_menu_idx = i;
        break;
      }
    settings_build_vis_rows();
  }
  /* Sync cubevol's stored backlight before checking the daemon. If it had
   * genuinely died, the replacement reads the right value immediately. */
  cube_pmem_backlight_sync(settings_brightness);
  fb1_set_visible(1); /* leave the live volume/input daemon untouched */
  settings_apply();
  settings_bl_reassert =
      120; /* re-assert safety net past cubevol's delayed apply */
  settings_audio_mute_reassert = settings_menu_sounds ? 0 : 120;
  dbg("settings loaded");
  recent_games_init();
  dbg("recent_games_init done");
  favorites_init();
  dbg("favorites_init done");
  core_override_load();
  ext_filter_load();
  build_core_choices();
  dbg("core overrides loaded");

  framebuffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(uint16_t));
  dbg("calloc done");
  render_init(framebuffer);
  dbg("render_init done");
  /* Resolve the roms root before the first scan (see g_roms_path). */
  resolve_roms_root();
  if (strcmp(g_roms_path, ROMS_PATH_DEFAULT) == 0) {
    DIR *d = opendir(g_roms_path);
    if (d)
      closedir(d);
    else {
      DIR *alt = opendir(SDCARD_BASE "/ROMS");
      if (alt) {
        closedir(alt);
        strcpy(g_roms_path, SDCARD_BASE "/ROMS");
      } else
        mkdir(g_roms_path, 0777);
    }
  }
  strncpy(current_path, g_roms_path, MAX_PATH_LEN - 1);
  current_path[MAX_PATH_LEN - 1] = '\0';
  scan_directory(current_path);
  dbg("scan_directory done");
  /* Recents reuses entries[] for its own rows. Keep the freshly-built Games
   * root in RAM so switching tabs never re-walks the FAT filesystem. */
  save_games_tab_entries();
  if (settings_load_recents && recent_games_get_count() > 0)
    enter_recents_view(); /* "Start in Recents" setting */
  shutdown_requested = false;
  dbg("retro_init complete");
}

void retro_deinit(void) {
  for (int i = 0; i < SYSTEM_ICON_CACHE_N; i++) {
    free(system_icons[i].pixels);
    free(system_icons[i].alpha);
    system_icons[i].pixels = NULL;
    system_icons[i].alpha = NULL;
  }
  system_icon_pack = -1;
  system_icon_missing_count = 0;
  free(framebuffer);
  framebuffer = NULL;
  free(view_transition_old);
  view_transition_old = NULL;
  free(view_transition_out);
  view_transition_out = NULL;
  free(games_tab_entries);
  games_tab_entries = NULL;
  games_tab_entry_count = 0;
  free(entries);
  entries = NULL;
  entry_count = entry_capacity = 0;
}

bool retro_load_game(const struct retro_game_info *info) {
  (void)info;
  return true; /* supports no-game */
}

void retro_unload_game(void) {}







void retro_run(void) {
  /* Take input from the libretro frontend (rkgame feeds cores this way; the
   * cubevol joy_key shm isn't updated when rkgame owns evdev). Build FrogUI's
   * raw bit layout from the joypad → input_set_ext_raw (OR'd with shm). */
  if (input_state_cb) {
    if (input_poll_cb)
      input_poll_cb();
    uint32_t raw = 0;
#define RB(id, bit)                                                            \
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, (id)))                         \
  raw |= (1u << (bit))
    RB(RETRO_DEVICE_ID_JOYPAD_UP, 4);
    RB(RETRO_DEVICE_ID_JOYPAD_DOWN, 6);
    RB(RETRO_DEVICE_ID_JOYPAD_LEFT, 7);
    RB(RETRO_DEVICE_ID_JOYPAD_RIGHT, 5);
    RB(RETRO_DEVICE_ID_JOYPAD_A, 13);
    RB(RETRO_DEVICE_ID_JOYPAD_B, 14);
    RB(RETRO_DEVICE_ID_JOYPAD_X, 12);
    RB(RETRO_DEVICE_ID_JOYPAD_Y, 15);
    RB(RETRO_DEVICE_ID_JOYPAD_L, 10);
    RB(RETRO_DEVICE_ID_JOYPAD_R, 11);
    RB(RETRO_DEVICE_ID_JOYPAD_START, 3);
    RB(RETRO_DEVICE_ID_JOYPAD_SELECT, 0);
#undef RB
    input_set_ext_raw(raw);
  }
  int old_selected = selected_index;
  int old_settings_row = settings_menu_idx;
  bool old_settings_active = settings_menu_active;
  bool old_recents = viewing_recents, old_favourites = viewing_favourites;
  char old_path[MAX_PATH_LEN];
  strncpy(old_path, current_path, sizeof old_path - 1);
  old_path[sizeof old_path - 1] = '\0';
  handle_input();
  if (old_selected != selected_index || old_settings_row != settings_menu_idx ||
      old_settings_active != settings_menu_active ||
      old_recents != viewing_recents || old_favourites != viewing_favourites ||
      strcmp(old_path, current_path) != 0)
    ui_menu_tick();

  /* Entering the systems screen can also happen after a folder, Settings or
   * Recents re-scan changed selected_index. Start that screen settled rather
   * than carrying a half-finished offset from its previous visit. */
  {
    static bool carousel_view_last = false;
    bool carousel_view_now = horizontal_system_view();
    if (carousel_view_now && !carousel_view_last) {
      system_carousel_frame = SYSTEM_CAROUSEL_FRAMES + 1;
      system_carousel_start_offset = 0.0f;
      system_carousel_visible_offset = 0.0f;
    }
    carousel_view_last = carousel_view_now;
  }

  /* Re-assert brightness every frame for a short window after boot so
   * cubevol's delayed startup apply of its own (default) stored value is
   * overwritten within one frame instead of flashing visibly. */
  if (settings_bl_reassert > 0) {
    cube_set_backlight(settings_brightness);
    settings_bl_reassert--;
  }
  /* Live-volume tail commit: the debounced slider path (500 ms) can leave
   * the newest level applied to the hardware mirror but not yet persisted
   * when the user stops sliding.  Commit it once the window has passed, so
   * the stored value (physical meter, next boot) always catches up without
   * waiting for a menu exit. */
  if (settings_volume != volume_committed_level &&
      monotonic_ms() - volume_last_commit_ms >= VOLUME_COMMIT_MS) {
    volume_committed_level = -1; /* force the real write below */
    settings_volume_commit_live();
  }
  /* Reverse link: the physical volume buttons (cubevol) change the stored
   * pmem value out from under us at any moment.  Poll it while the
   * Settings menu is open so the slider DISPLAY follows the buttons in
   * real time too - one value everywhere, whichever side changed it.
   * (picoarch does the same for the in-game path; this covers the UI.)
   * OWN-LAG GUARD: while the debounced slider path is catching up, pmem
   * still holds OUR last committed level - reading it back would bounce
   * the slider to stale values mid-adjustment (seen on-device as
   * 50-45-45-45-40-45-40 when sliding fast).  A read that matches our own
   * last commit is our lag, not a physical press: only adopt values that
   * differ from BOTH the slider and our last commit. */
  if (settings_menu_active) {
    static long long volume_poll_ms = 0;
    long long now = monotonic_ms();
    if (now - volume_poll_ms >= 250) {
      volume_poll_ms = now;
      int shared = cube_pmem_volume_read();
      if (shared >= 0 && shared <= 100 && shared != settings_volume &&
          shared != volume_committed_level) {
        settings_volume = shared;
        volume_committed_level = shared; /* it is already stored */
      }
    }
  }
  /* cubevol can restore its stored I2SO level shortly after boot.  Keep the
     idle launcher mute asserted through that window, without changing the
     stored level used for the next game launch. */
  if (settings_audio_mute_reassert > 0) {
    cube_set_i2so_output_muted(1);
    settings_audio_mute_reassert--;
  }

  /* Reload banner when view, path, or selection changes.
   * On the main SYSTEMS menu, preview the highlighted folder's banner. */
  static char banner_last_path[MAX_PATH_LEN] = "";
  static bool banner_last_recents = false;
  static bool banner_last_favourites = false;
  static int banner_last_sel = -1;
  static int banner_last_bg = -1; /* reload when the backgrounds toggle flips */
  static int banner_last_wp = -1; /* reload when the wallpaper choice changes */
  static int banner_last_wpfit =
      -1; /* reload when the wallpaper fit mode changes */
  static int banner_last_pack =
      -1;                          /* reload when the background pack changes */
  static int banner_last_dim = -1; /* reload when background dim changes */
  {
    const char *banner_path = current_path;
    char sel_path[MAX_PATH_LEN];
    if (settings_menu_active) {
      banner_path = "settings";
    } else if (!viewing_recents && !viewing_favourites && selected_index >= 0 &&
               selected_index < entry_count) {
      if (strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) == 0) {
        banner_path = "settings";
      } else if (strcmp(current_path, ROMS_PATH) == 0 &&
                 strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) ==
                     0) {
        banner_path = "recents";
      } else if (strcmp(current_path, ROMS_PATH) == 0 &&
                 strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) ==
                     0) {
        banner_path = "favourites";
      } else if (entries[selected_index].is_dir &&
                 strcmp(current_path, ROMS_PATH) == 0) {
        snprintf(sel_path, sizeof(sel_path), "%s/%s", current_path,
                 entries[selected_index].name);
        banner_path = sel_path;
      }
    }
    /* Reload ONLY when the background actually changes.
     * - A single WALLPAPER is the same image in every view: decode it ONCE
     *   and never reload on folder/view/selection change (only when the
     *   wallpaper choice, fit, or backgrounds toggle changes). This was the
     *   scroll killer - the wallpaper was re-decoded + re-scaled on every
     *   folder change.
     * - Otherwise the per-system art depends on banner_path (which already
     *   encodes root folder-preview + special entries), never selected_index
     *   directly. */
    (void)banner_last_sel;
    int wp_active = settings_backgrounds && settings_wallpaper_idx > 0;
    int need_reload;
    if (wp_active) {
      need_reload = (settings_wallpaper_idx != banner_last_wp ||
                     settings_wallpaper_fit != banner_last_wpfit ||
                     settings_theme_pack_idx != banner_last_pack ||
                     settings_background_dim != banner_last_dim ||
                     settings_backgrounds != banner_last_bg);
    } else {
      need_reload = (viewing_recents != banner_last_recents ||
                     viewing_favourites != banner_last_favourites ||
                     strcmp(banner_path, banner_last_path) != 0 ||
                     settings_backgrounds != banner_last_bg ||
                     settings_wallpaper_idx != banner_last_wp ||
                     settings_wallpaper_fit != banner_last_wpfit ||
                     settings_theme_pack_idx != banner_last_pack ||
                     settings_background_dim != banner_last_dim);
    }
    if (need_reload && !(viewing_recents && settings_game_switcher)) {
      /* GameSwitcher paints its own full panel. Leave the Games banner
       * and its cache keys untouched while Recents is open, so returning
       * to Games does not decode the same background from SD again. */
      load_banner_for_view(banner_path, viewing_recents, viewing_favourites);
      banner_last_recents = viewing_recents;
      banner_last_favourites = viewing_favourites;
      banner_last_sel = selected_index;
      banner_last_bg = settings_backgrounds;
      banner_last_wp = settings_wallpaper_idx;
      banner_last_wpfit = settings_wallpaper_fit;
      banner_last_pack = settings_theme_pack_idx;
      banner_last_dim = settings_background_dim;
      strncpy(banner_last_path, banner_path, MAX_PATH_LEN - 1);
      banner_last_path[MAX_PATH_LEN - 1] = '\0';
    }
  }

  /* Redraw-only-when-dirty for the browser and Settings. Most calls have no
   * visual change,
   * so neither recompose nor re-present the old frame: R36SX's presenter waits
   * for vsync (~16.7ms), and calling it while idle needlessly limits input
   * polling to 60Hz. A short sleep keeps the idle loop cheap while still
   * sampling input at roughly 1kHz. Search/picker/remap overlays remain
   * continuously drawn for now. */
  int can_skip_idle =
      !(search_kbd_active || core_picker_active || remap_wizard_active);
  int redraw = 1;
  if (can_skip_idle) {
    static unsigned last_sig = 0;
    static int first = 1;
    unsigned sig = 5381u;
    sig = sig * 33u + (unsigned)ui_toast_frames;
    sig = sig * 33u + (unsigned)view_transition_frame;
    /* Include modal state in the presentation signature.  Without this,
     * B correctly cancelled USB mode but the compositor skipped the next
     * frame, leaving the old "USB MTP READY" framebuffer on screen until
     * an unrelated navigation key happened to invalidate it. */
    sig = sig * 33u + (unsigned)usb_mode_confirm_active;
    sig = sig * 33u + (unsigned)usb_mode_initiated_active;
    sig = sig * 33u + (unsigned)settings_menu_active;
    if (settings_menu_active) {
      sig = sig * 33u + (unsigned)settings_menu_idx;
      /* Collapsed/expanded section state: pressing A on a header changes
       * NO row value and NO cursor index ??? without these bits in the
       * signature the screen would wait for the next cursor move. */
      for (int s = 0; s < (int)(sizeof(settings_section_open) /
                                sizeof(settings_section_open[0]));
           s++)
        sig = sig * 33u + (settings_section_open[s] ? 1u : 0u);
      sig = sig * 33u + (unsigned)settings_theme_idx;
      sig = sig * 33u + (unsigned)settings_style;
      sig = sig * 33u + (unsigned)settings_icon_pack_idx;
      sig = sig * 33u + (unsigned)settings_font_idx;
      sig = sig * 33u + (unsigned)settings_wallpaper_idx;
      sig = sig * 33u + (unsigned)settings_wallpaper_fit;
      for (int i = 0; i < SETTINGS_ROW_N; i++)
        if (settings_rows[i].val)
          sig = sig * 33u + (unsigned)*settings_rows[i].val;
    } else {
      sig = sig * 33u + (unsigned)selected_index;
      sig = sig * 33u + (unsigned)scroll_offset;
      sig =
          sig * 33u + (unsigned)(viewing_recents * 16 + viewing_favourites * 8 +
                                 viewing_apps * 4 + apps_browsing * 2 +
                                 viewing_activity + viewing_search * 32);
      sig = sig * 33u + (unsigned)game_switcher_fullscreen;
      for (const char *p = current_path; *p; p++)
        sig = sig * 33u + (unsigned char)*p;
    }
    redraw = first || sig != last_sig || banner_is_animating() ||
             system_carousel_is_animating() ||
             view_transition_frame <= VIEW_TRANSITION_FRAMES ||
             ui_toast_frames > 0 || usb_mode_confirm_active ||
             usb_mode_initiated_active;
    last_sig = sig;
    first = 0;
  }
  if (!redraw) {
    /* Catch cubevol's asynchronous charging repaint within roughly one
     * display frame. The mmap and geometry stay cached. */
    static unsigned fb1_idle_ticks = 0;
    if (++fb1_idle_ticks >= 16) {
      fb1_idle_ticks = 0;
      fb1_clear_battery_zone();
    }
    usleep(1000);
    return;
  }
  /* Clear immediately on the first carousel frame and every real redraw.
   * Only the top-right battery zone is touched; cubevol's centered volume
   * popup remains available. */
  fb1_clear_battery_zone();

  if (usb_mode_confirm_active || usb_mode_initiated_active) {
    render_usb_confirm();
  } else if (search_kbd_active) {
    render_search_kbd();
  } else if (extf_kbd_active) {
    render_extf_kbd();
  } else if (core_picker_active) {
    render_core_picker();
  } else if (remap_wizard_active) {
    render_remap_wizard();
  } else if (settings_menu_active) {
    render_settings_menu();
  } else if (viewing_recents && settings_game_switcher) {
    /* Do not paint or discard the cached Games banner behind GameSwitcher. */
    render_clear_screen(framebuffer);
    render_game_switcher(framebuffer);
  } else {
    if (banner_is_loaded())
      banner_render(framebuffer);
    else
      render_clear_screen(framebuffer);
    if (horizontal_system_view()) {
      render_system_carousel(framebuffer);
    } else if (icon_system_view()) {
      render_system_grid(framebuffer);
    } else {
      static char search_title[96];
      const char *title;
      if (viewing_search) {
        snprintf(search_title, sizeof(search_title), "SEARCH: %s (%d)",
                 search_query, entry_count);
        title = search_title;
      } else {
        title = viewing_activity     ? app_label("activity")
                : viewing_recents    ? "RECENT GAMES"
                : viewing_apps       ? "APPS"
                : viewing_favourites ? "FAVOURITES"
                : (strcmp(current_path, ROMS_PATH) == 0)
                    ? "TREEFROGUI: SYSTEMS"
                    : system_display_name(get_basename(current_path));
      }
      if (viewing_activity)
        render_tabs(framebuffer, MAIN_TAB_APPS, COLOR_BG);
      else if (viewing_recents)
        render_tabs(framebuffer, MAIN_TAB_RECENTS, COLOR_BG);
      else if (viewing_apps)
        render_tabs(framebuffer, MAIN_TAB_APPS, COLOR_BG);
      else if (strcmp(current_path, ROMS_PATH) == 0 && !viewing_favourites &&
               !viewing_search)
        render_tabs(framebuffer, MAIN_TAB_GAMES, COLOR_BG);
      else
        render_header(framebuffer, title);
      {
        int visible = min(entry_count - scroll_offset, VISIBLE_ENTRIES);
        for (int i = 0; i < visible; i++) {
          int idx = scroll_offset + i;
          const char *shown = entries[idx].name;
          char disp[256];
          if (viewing_apps && entries[idx].is_dir)
            shown = app_label(entries[idx].name);
          else if (entries[idx].is_dir && strcmp(current_path, ROMS_PATH) == 0)
            shown = system_display_name(entries[idx].name);
          /* gamelist.xml <name> replaces the file name for file rows (no
           * extension stripping); entries[].name stays the real file name. */
          const char *gl_name = NULL;
          if (!entries[idx].is_dir && !viewing_recents && !viewing_favourites &&
              !viewing_search && !viewing_apps)
            gl_name = gamelist_title_for(current_path, entries[idx].name);
          if (gl_name)
            shown = gl_name;
          /* Hide file extension (.gb/.gba/...) when enabled — display only,
           * the real name in entries[] is still used to launch. Files only. */
          else if (settings_hide_extensions && !entries[idx].is_dir) {
            strncpy(disp, entries[idx].name, sizeof disp - 1);
            disp[sizeof disp - 1] = '\0';
            char *dot = strrchr(disp, '.');
            if (dot && dot != disp) {
              *dot = '\0';
              shown = disp;
            }
          }
          if (settings_center_text && settings_style == STYLE_VERTICAL &&
              strcmp(current_path, ROMS_PATH) == 0)
            render_menu_item_centered(framebuffer, idx, shown,
                                      entries[idx].is_dir,
                                      (idx == selected_index), scroll_offset);
          else
            render_menu_item(framebuffer, idx, shown, entries[idx].is_dir,
                             (idx == selected_index), scroll_offset, 0);
        }
        render_scroll_indicator(framebuffer, entry_count, selected_index,
                                VISIBLE_ENTRIES);
      }
      /* Box-art panel for the selected game (normal browsing only). */
      if (!viewing_recents && !viewing_favourites && !viewing_search &&
          selected_index >= 0 && selected_index < entry_count &&
          !entries[selected_index].is_dir &&
          strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
          strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
          strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) != 0) {
        char fp[1024];
        snprintf(fp, sizeof fp, "%s/%s", current_path,
                 entries[selected_index].name);
        render_boxart_panel(framebuffer, fp, entries[selected_index].name);
      }

      /* Compute Y-button legend mode for current selection */
      int legend_mode = LEGEND_X_NONE;
      if (viewing_favourites) {
        legend_mode = LEGEND_X_REMOVE;
      } else if (!viewing_recents && selected_index < entry_count &&
                 !entries[selected_index].is_dir &&
                 strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) !=
                     0 &&
                 strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) !=
                     0 &&
                 strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) !=
                     0) {
        const char *folder = get_console_folder(current_path);
        const char *core = get_core_for_folder(folder);
        if (!core)
          core = get_core_for_extension(entries[selected_index].name);
        if (core) {
          char game_name[256];
          strncpy(game_name, entries[selected_index].name,
                  sizeof(game_name) - 1);
          game_name[sizeof(game_name) - 1] = '\0';
          char *dot = strrchr(game_name, '.');
          if (dot)
            *dot = '\0';
          legend_mode = favorites_is_favorited(core, game_name)
                            ? LEGEND_X_REMOVE
                            : LEGEND_X_FAVOURITE;
        }
      }
      /* SELECT opens the core picker on folders + real ROMs (matches the SELECT
       * handler guard): show the "SEL-OPTIONS" hint there. */
      int show_select =
          (!viewing_recents && !viewing_favourites && !viewing_search &&
           selected_index < entry_count &&
           strcmp(entries[selected_index].name, SETTINGS_ENTRY_NAME) != 0 &&
           strcmp(entries[selected_index].name, RECENTS_ENTRY_NAME) != 0 &&
           strcmp(entries[selected_index].name, FAVOURITES_ENTRY_NAME) != 0);
      /* X opens search in the normal browser (systems root + any game folder).
       */
      int show_search =
          (!viewing_recents && !viewing_favourites && !viewing_search);

      /* Play-time for the selected game (Recents list only). */
      if (viewing_recents && selected_index < entry_count &&
          selected_index < recent_games_get_count()) {
        const RecentGame *rg = recent_games_get_list();
        long s = playtime_lookup(rg[selected_index].full_path);
        if (s > 0) {
          char t[64];
          long h = s / 3600, m = (s % 3600) / 60;
          if (h)
            snprintf(t, sizeof t, "Played %ldh %ldm", h, m);
          else if (s >= 60)
            snprintf(t, sizeof t, "Played %ldm", m);
          else
            snprintf(t, sizeof t, "Played %lds", s);
          font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                         SCREEN_HEIGHT - 56, t, COLOR_TEXT);
        }
      }
      if (viewing_activity && selected_index < activity_count) {
        long s = activity_seconds[selected_index];
        char t[128];
        long h = s / 3600, m = (s % 3600) / 60;
        if (h)
          snprintf(t, sizeof t, "%ld runs  %ldh %ldm",
                   activity_runs[selected_index], h, m);
        else if (s >= 60)
          snprintf(t, sizeof t, "%ld runs  %ldm", activity_runs[selected_index],
                   s / 60);
        else
          snprintf(t, sizeof t, "%ld runs  %lds", activity_runs[selected_index],
                   s);
        if (activity_last[selected_index] > 0) {
          char date[32];
          struct tm tmv;
          localtime_r(&activity_last[selected_index], &tmv);
          strftime(date, sizeof date, "%Y-%m-%d", &tmv);
          size_t used = strlen(t);
          snprintf(t + used, sizeof t - used, "  last %s", date);
        } else {
          size_t used = strlen(t);
          snprintf(t + used, sizeof t - used, "  all time");
        }
        font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                       SCREEN_HEIGHT - 56, t, COLOR_TEXT);
      }
      if (viewing_activity)
        render_activity_page(framebuffer);

      render_legend(framebuffer, legend_mode, show_select, show_search);
    }
  }

  if (ui_toast_frames > 0) {
    render_toast(framebuffer, ui_toast_text);
    ui_toast_frames--;
  }

  const uint16_t *present = ui_transition_compose();
  if (video_cb)
    video_cb(present, SCREEN_WIDTH, SCREEN_HEIGHT,
             SCREEN_WIDTH * sizeof(uint16_t));
}

void retro_reset(void) {
  scan_directory(ROMS_PATH);
  strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *d, size_t s) {
  (void)d;
  (void)s;
  return false;
}
bool retro_unserialize(const void *d, size_t s) {
  (void)d;
  (void)s;
  return false;
}
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char *c) {
  (void)i;
  (void)e;
  (void)c;
}
void *retro_get_memory_data(unsigned id) {
  (void)id;
  return NULL;
}
size_t retro_get_memory_size(unsigned id) {
  (void)id;
  return 0;
}
bool retro_load_game_special(unsigned t, const struct retro_game_info *i,
                             size_t n) {
  (void)t;
  (void)i;
  (void)n;
  return false;
}
