#include "../app/app.h" // IWYU pragma: keep

/* Folder names remain the stable lookup key for cores and artwork. The
 * horizontal picker gets friendlier names without changing that contract. */
static SystemLabel system_labels[] = {{"a26", "Atari 2600"},
                                      {"a5200", "Atari 5200"},
                                      {"a78", "Atari 7800"},
                                      {"a800", "Atari 8-bit"},
                                      {"lnx", "Atari Lynx"},
                                      {"fc", "Nintendo Entertainment System"},
                                      {"nes", "Nintendo Entertainment System"},
                                      {"nesq", "NES - QuickNES"},
                                      {"nest", "NES - Nestopia"},
                                      {"fds", "Famicom Disk System"},
                                      {"sfc", "Super Nintendo"},
                                      {"snes", "Super Nintendo"},
                                      {"snes02", "SNES - Fast"},
                                      {"gb", "Game Boy"},
                                      {"gbc", "Game Boy Color"},
                                      {"gbgb", "Game Boy - Gearboy"},
                                      {"gbb", "Game Boy - TGB Dual"},
                                      {"dblcherrygb", "Double Cherry GB"},
                                      {"gba", "Game Boy Advance"},
                                      {"gbac", "GBA - Multicore"},
                                      {"gbav", "GBA - VBA Next"},
                                      {"gbaf", "GBA - mGBA"},
                                      {"mgba", "GBA - mGBA"},
                                      {"md", "Mega Drive / Genesis"},
                                      {"sms", "Master System"},
                                      {"sega", "Mega Drive / Genesis"},
                                      {"gg", "Game Gear"},
                                      {"gpgx", "Genesis Plus GX"},
                                      {"segacd", "Sega CD"},
                                      {"32x", "Sega 32X"},
                                      {"pce", "PC Engine"},
                                      {"pcesgx", "PC Engine SuperGrafx"},
                                      {"ngpc", "Neo Geo Pocket"},
                                      {"neogeo", "Neo Geo"},
                                      {"geolith", "Neo Geo"},
                                      {"wswan", "WonderSwan"},
                                      {"wsv", "WonderSwan - Potator"},
                                      {"vb", "Virtual Boy"},
                                      {"pcfx", "PC-FX"},
                                      {"ps", "PlayStation"},
                                      {"psx", "PlayStation"},
                                      {"ps1", "PlayStation"},
                                      {"ps1r", "PlayStation - ReARMed"},
                                      {"qpsx", "PlayStation - QPSX"},
                                      {"psp", "PlayStation Portable"},
                                      {"nds", "Nintendo DS"},
                                      {"arcade", "Arcade"},
                                      {"m2k", "Arcade - MAME 2000"},
                                      {"cps1", "Arcade - Capcom CPS-1"},
                                      {"cps2", "Arcade - Capcom CPS-2"},
                                      {"cps3", "Arcade - Capcom CPS-3"},
                                      {"c64", "Commodore 64"},
                                      {"c64sc", "Commodore 64"},
                                      {"c64f", "Commodore 64 - Frodo"},
                                      {"c64fc", "Commodore 64 - Frodo"},
                                      {"amiga", "Commodore Amiga"},
                                      {"vic20", "Commodore VIC-20"},
                                      {"atari-st", "Atari ST"},
                                      {"msx", "MSX"},
                                      {"msx2", "MSX2"},
                                      {"spec", "ZX Spectrum"},
                                      {"zx", "ZX Spectrum"},
                                      {"zx81", "ZX81"},
                                      {"col", "ColecoVision"},
                                      {"amstrad", "Amstrad CPC"},
                                      {"amstradb", "Amstrad CPC Plus"},
                                      {"thom", "Thomson MO / TO"},
                                      {"xmil", "Sharp X68000"},
                                      {"pico286", "DOS / PC"},
                                      {"j2me", "Java Games"},
                                      {"dos", "DOS"},
                                      {"prboom", "Doom"},
                                      {"doom", "Doom"},
                                      {"quake", "Quake"},
                                      {"quake2", "Quake II"},
                                      {"wolf3d", "Wolfenstein 3D"},
                                      {"outrun", "Out Run"},
                                      {"cavestory", "Cave Story"},
                                      {"flashback", "Flashback"},
                                      {"xrick", "Rick Dangerous"},
                                      {"jnb", "Jump 'n Bump"},
                                      {"gw", "Game & Watch"},
                                      {"tic80", "TIC-80"},
                                      {"pico8", "PICO-8"},
                                      {"fake08", "PICO-8"},
                                      {"retro8", "PICO-8 - Retro8"},
                                      {"lowres-nx", "LowRes NX"},
                                      {"pokem", "Pokemon Mini"},
                                      {"int", "Intellivision"},
                                      {"fcf", "Fairchild Channel F"},
                                      {"cdg", "CD+G Karaoke"},
                                      {"chip8", "CHIP-8"},
                                      {"arduboy", "Arduboy"},
                                      {"arduous", "Arduboy - Accurate"},
                                      {"vec", "Vectrex"},
                                      {"o2em", "Odyssey 2"},
                                      {"gme", "Game Music"},
                                      {"gong", "Pong"},
                                      {"vapor", "VaporSpec"},
                                      {"rockbox", "Rockbox"},
                                      {"music", "Music Player"},
                                      {"audio", "Music Player"},
                                      {"videos", "Videos"},
                                      {"video", "Videos"},
                                      {"images", "Images"},
                                      {"photos", "Images"}};
/* Keep friendly labels as a stable snapshot for the active locale. This keeps
 * the Games hierarchy independent of the in-place JSON parser buffer and
 * makes a language switch an explicit update rather than a deferred lookup. */
void system_labels_refresh(void) {
  for (size_t i = 0; i < sizeof(system_labels) / sizeof(system_labels[0]);
       i++) {
    char key[96];
    snprintf(key, sizeof(key), "system.%s", system_labels[i].folder);
    snprintf(system_labels[i].label, sizeof(system_labels[i].label), "%s",
             tr_or(key, system_labels[i].label_en));
  }
}
const char *system_display_name(const char *folder) {
  if (strcmp(folder, SETTINGS_ENTRY_NAME) == 0)
    return tr("folder.settings");
  if (strcmp(folder, RECENTS_ENTRY_NAME) == 0)
    return tr("folder.recents");
  if (strcmp(folder, FAVOURITES_ENTRY_NAME) == 0)
    return tr("folder.favourites");
  if (!settings_friendly_names)
    return folder;
  for (size_t i = 0; i < sizeof(system_labels) / sizeof(system_labels[0]); i++)
    if (strcasecmp(folder, system_labels[i].folder) == 0) {
      return system_labels[i].label[0] ? system_labels[i].label
                                       : system_labels[i].label_en;
    }
  return folder;
}
void load_banner_for_view(const char *path, bool is_recents,
                          bool is_favourites) {
  if (!settings_backgrounds) {
    banner_clear();
    return;
  } /* solid theme bg */
  /* Single wallpaper (if picked): same image in every view, overrides the
   * per-system art. */
  if (settings_wallpaper_idx > 0 && settings_wallpaper_idx < wallpaper_count) {
    char wp[512];
    snprintf(wp, sizeof wp, "/mnt/sdcard/frogui/wallpapers/%s",
             wallpaper_files[settings_wallpaper_idx]);
    if (access(wp, R_OK) == 0) {
      banner_load_fit(wp, settings_wallpaper_fit, COLOR_BG);
      return;
    }
  }
  char img[512];
  char banner_dir[512];
  if (settings_theme_pack_idx > 0 && settings_theme_pack_idx < theme_pack_count)
    snprintf(banner_dir, sizeof banner_dir, BANNER_DIR "/theme-packs/%s",
             theme_pack_files[settings_theme_pack_idx]);
  else
    snprintf(banner_dir, sizeof banner_dir, "%s", BANNER_DIR);
  const char *exts[] = {"png", "jpg", "jpeg", "bmp", NULL};
  if (is_recents) {
    for (int i = 0; exts[i]; i++) {
      snprintf(img, sizeof(img), "%s/recents.%s", banner_dir, exts[i]);
      if (access(img, R_OK) == 0) {
        banner_load_fit(img, settings_wallpaper_fit, COLOR_BG);
        return;
      }
    }
  } else if (is_favourites) {
    for (int i = 0; exts[i]; i++) {
      snprintf(img, sizeof(img), "%s/favourites.%s", banner_dir, exts[i]);
      if (access(img, R_OK) == 0) {
        banner_load_fit(img, settings_wallpaper_fit, COLOR_BG);
        return;
      }
    }
  } else {
    const char *base = (path && *path) ? strrchr(path, '/') : NULL;
    const char *name = base ? base + 1 : (path ? path : "main");
    if (!name || !*name)
      name = "main";
    /* Theme packs historically used m2k/gb names. Keep those assets
     * working for the canonical arcade and Game Boy Color folders too. */
    const char *names[] = {
        name, strcasecmp(name, "arcade") == 0 ? "m2k" : NULL,
        strcasecmp(name, "gbc") == 0 ? "gb" : NULL,
        /* Java Games has no dedicated artwork; use the active theme's
         * composed main banner so stripes/framing remain intact. */
        strcasecmp(name, "j2me") == 0 ? "main" : NULL,
        strcasecmp(name, "qpsx") == 0 ? "ps1" : NULL, NULL};
    for (int n = 0; names[n]; n++)
      for (int i = 0; exts[i]; i++) {
        snprintf(img, sizeof(img), "%s/%s.%s", banner_dir, names[n], exts[i]);
        if (access(img, R_OK) == 0) {
          banner_load_fit(img, settings_wallpaper_fit, COLOR_BG);
          return;
        }
      }
    /* Fallback: try main.png for any view that has no folder-specific image */
    for (int i = 0; exts[i]; i++) {
      snprintf(img, sizeof(img), "%s/main.%s", banner_dir, exts[i]);
      if (access(img, R_OK) == 0) {
        banner_load_fit(img, settings_wallpaper_fit, COLOR_BG);
        return;
      }
    }
  }
  banner_clear();
}
