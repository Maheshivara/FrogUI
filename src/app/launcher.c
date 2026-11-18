#include "app.h" // IWYU pragma: keep

/* internal helpers */
static unsigned rgb565_to_888(uint16_t c);
static void write_picoarch_skin(void);

/* Hand the active theme's colors to picoarch so its in-game menu matches
 * FrogUI. picoarch reads <exe_dir>/skin/skin.txt; its parser expects 24-bit
 * RGB888 hex (it converts to RGB565), so expand our RGB565 theme colors to
 * RGB888. */
static unsigned rgb565_to_888(uint16_t c) {
  unsigned r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
  unsigned r8 = (r5 << 3) | (r5 >> 2);
  unsigned g8 = (g6 << 2) | (g6 >> 4);
  unsigned b8 = (b5 << 3) | (b5 >> 2);
  return (r8 << 16) | (g8 << 8) | b8;
}
static void write_picoarch_skin(void) {
  extern uint16_t theme_text(void);
  extern uint16_t theme_select_text(void);
  extern uint16_t theme_select_bg(void);
  mkdir("/mnt/sdcard/cubegm/skin", 0777);
  FILE *s = fopen("/mnt/sdcard/cubegm/skin/skin.txt", "w");
  if (!s)
    return;
  /* Match FrogUI: normal rows in theme text colour, selected row = select-text
   * on the select-bg pill. */
  fprintf(s, "text_color=0x%06X\n", rgb565_to_888(theme_text()));
  fprintf(s, "selection_color=0x%06X\n", rgb565_to_888(theme_select_bg()));
  fprintf(s, "sel_text_color=0x%06X\n", rgb565_to_888(theme_select_text()));
  fprintf(s, "background_color=0x%06X\n", rgb565_to_888(theme_bg()));
  fclose(s);
}
void request_game_launch(const char *core_path, const char *rom_path) {
  fb1_clear_all();
  cube_set_i2so_output_muted(0); /* game owns the audio path after exec */
  write_picoarch_skin();
  FILE *f = fopen(LAUNCH_FILE, "w");
  if (!f) {
    dbg("failed to write launch file");
    return;
  }
  fprintf(f, "%s\n%s\n", core_path, rom_path);
  fclose(f);
  /* Record in recent games history */
  const char *rom_base = strrchr(rom_path, '/');
  rom_base = rom_base ? rom_base + 1 : rom_path;
  char game_name[256];
  strncpy(game_name, rom_base, sizeof(game_name) - 1);
  game_name[sizeof(game_name) - 1] = '\0';
  char *dot = strrchr(game_name, '.');
  if (dot)
    *dot = '\0';
  recent_games_add(core_path, game_name, rom_path);
  sync(); /* flush FAT32 before exec */
  dbg("launch file written, requesting shutdown");
  if (environ_cb)
    environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}
void request_standalone_launch(const char *bin_path, const char *rom_path) {
  dbg("standalone_launch: start");
  fb1_clear_all();
  cube_set_i2so_output_muted(0); /* standalone app owns the audio path */
  write_picoarch_skin();
  FILE *f = fopen(LAUNCH_FILE, "w");
  if (!f) {
    dbg("standalone_launch: fopen failed");
    return;
  }
  fprintf(f, "standalone\n%s\n%s\n", bin_path, rom_path);
  fclose(f);
  dbg("standalone_launch: file written");
  const char *rom_base = strrchr(rom_path, '/');
  rom_base = rom_base ? rom_base + 1 : rom_path;
  char game_name[256];
  strncpy(game_name, rom_base, sizeof(game_name) - 1);
  game_name[sizeof(game_name) - 1] = '\0';
  char *dot = strrchr(game_name, '.');
  if (dot)
    *dot = '\0';
  /* Media playback is an app, not a game: keep MP3/video launches out of
   * Recents so that list remains useful for actual games. */
  if (strcmp(bin_path, ROCKBOX_BIN) != 0 && strcmp(bin_path, VIDEO_BIN) != 0 &&
      strcmp(bin_path, FROGSHELL_CORE) != 0) {
    dbg("standalone_launch: calling recent_games_add");
    recent_games_add(bin_path, game_name, rom_path);
  } else {
    dbg("standalone_launch: media app, skipping recents");
  }
  dbg("standalone_launch: calling environ_cb SHUTDOWN");
  if (environ_cb)
    environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
  dbg("standalone_launch: after environ_cb");
}
/* Built-in utilities use the standalone exec contract but are not games and
 * therefore must not create Recents entries. */
void request_builtin_launch(const char *bin_path) {
  dbg("builtin_launch: start");
  fb1_clear_all();
  cube_set_i2so_output_muted(0); /* built-in standalone app may use audio */
  FILE *f = fopen(LAUNCH_FILE, "w");
  if (!f) {
    dbg("builtin_launch: fopen failed");
    return;
  }
  fprintf(f, "standalone\n%s\n\n", bin_path);
  fclose(f);
  sync();
  if (environ_cb)
    environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}
/* Single launch path used by browse, search, favourites, and recents. Resolves
 * everything from the ROM path so all four agree — favourites/recents used to
 * launch the stored core_name directly and so missed the ps1-folder →
 * standalone pcsx4all preference (a ps1 favourite booted pcsx_rearmed instead).
 */
void launch_by_path(const char *path) {
  const char *folder = get_console_folder(path);
  char dir[MAX_PATH_LEN];
  strncpy(dir, path, sizeof(dir) - 1);
  dir[sizeof(dir) - 1] = '\0';
  char *slash = strrchr(dir, '/');
  if (slash)
    *slash = '\0';
  const char *ov = core_override_lookup(path, dir);
  const char *core = ov ? ov : get_core_for_folder(folder);
  if (!core)
    core = get_core_for_extension(path);
  if (ov)
    request_game_launch(ov, path); /* explicit override → libretro */
  else if (core) {
    /* ps1 folder maps to pcsx_rearmed by default but we prefer the standalone
     * pcsx4all when present; every other standalone (pico286/lgpt/rockbox) is
     * caught generically by is_standalone_bin on the resolved core path. */
    if (is_ps1_folder(folder) && access(PCSX4ALL_BIN, F_OK) == 0)
      request_standalone_launch(PCSX4ALL_BIN, path);
    else if (is_psp_folder(folder) && access(PPSSPP_BIN, X_OK) == 0)
      request_standalone_launch(PPSSPP_BIN, path);
    else if (is_standalone_bin(core) && access(core, F_OK) == 0)
      request_standalone_launch(core, path);
    else
      request_game_launch(core, path);
  } else {
    dbg("launch: no core mapping for path");
  }
}
