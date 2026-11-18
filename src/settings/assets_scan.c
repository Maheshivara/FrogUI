#include "../app/app.h"

/* internal helpers */
static int font_has_ext(const char *name);
static void font_add(const char *fname);
static int wall_has_ext(const char *n);

char font_files[MAX_FONTS][FONT_STR_MAX];
char font_disp[MAX_FONTS][FONT_STR_MAX];
int font_count = 0;
static int font_has_ext(const char *name) {
  const char *dot = strrchr(name, '.');
  return dot && (strcasecmp(dot, ".ttf") == 0 || strcasecmp(dot, ".otf") == 0);
}
static void font_add(const char *fname) {
  if (font_count >= MAX_FONTS)
    return;
  for (int i = 0; i < font_count; i++)
    if (strcasecmp(font_files[i], fname) == 0)
      return; /* dedup */
  strncpy(font_files[font_count], fname, FONT_STR_MAX - 1);
  font_files[font_count][FONT_STR_MAX - 1] = '\0';
  font_count++;
}
char wallpaper_files[MAX_WALL][FONT_STR_MAX];
char wallpaper_disp[MAX_WALL][FONT_STR_MAX];
int wallpaper_count = 0; /* includes slot 0 = "None" */
const char *wallpaper_fit_names[] = {"Fill", "Fit", "Stretch", "Center",
                                     "Tile"};
char theme_pack_files[MAX_THEME_PACKS][FONT_STR_MAX];
char theme_pack_disp[MAX_THEME_PACKS][FONT_STR_MAX];
int theme_pack_count = 1;
char icon_pack_files[MAX_ICON_PACKS][FONT_STR_MAX];
char icon_pack_disp[MAX_ICON_PACKS][FONT_STR_MAX];
int icon_pack_count = 1;
void icon_pack_scan(void) {
  icon_pack_count = 1;
  strcpy(icon_pack_files[0], "");
  strcpy(icon_pack_disp[0], "Default (Cosy)");
  DIR *dp = opendir("/mnt/sdcard/frogui/icon-packs");
  if (!dp)
    return;
  struct dirent *e;
  while ((e = readdir(dp)) && icon_pack_count < MAX_ICON_PACKS) {
    if (e->d_name[0] == '.')
      continue;
    if (e->d_type != DT_DIR) {
      char full[512];
      struct stat st;
      snprintf(full, sizeof full, "/mnt/sdcard/frogui/icon-packs/%s",
               e->d_name);
      if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
        continue;
    }
    strncpy(icon_pack_files[icon_pack_count], e->d_name, FONT_STR_MAX - 1);
    icon_pack_files[icon_pack_count][FONT_STR_MAX - 1] = '\0';
    strncpy(icon_pack_disp[icon_pack_count], e->d_name, FONT_STR_MAX - 1);
    icon_pack_disp[icon_pack_count][FONT_STR_MAX - 1] = '\0';
    for (char *p = icon_pack_disp[icon_pack_count]; *p; p++)
      if (*p == '_')
        *p = ' ';
    icon_pack_count++;
  }
  closedir(dp);
  /* Stable alphabetical menu; slot 0 stays the default. */
  for (int i = 2; i < icon_pack_count; i++) {
    char kf[FONT_STR_MAX], kd[FONT_STR_MAX];
    strncpy(kf, icon_pack_files[i], FONT_STR_MAX);
    strncpy(kd, icon_pack_disp[i], FONT_STR_MAX);
    int j = i - 1;
    while (j >= 1 && strcasecmp(icon_pack_disp[j], kd) > 0) {
      strncpy(icon_pack_files[j + 1], icon_pack_files[j], FONT_STR_MAX);
      strncpy(icon_pack_disp[j + 1], icon_pack_disp[j], FONT_STR_MAX);
      j--;
    }
    strncpy(icon_pack_files[j + 1], kf, FONT_STR_MAX);
    strncpy(icon_pack_disp[j + 1], kd, FONT_STR_MAX);
  }
  if (settings_icon_pack_idx < 0 || settings_icon_pack_idx >= icon_pack_count)
    settings_icon_pack_idx = 0;
}
void theme_pack_scan(void) {
  theme_pack_count = 1;
  strcpy(theme_pack_files[0], "");
  strcpy(theme_pack_disp[0], "Default");
  DIR *dp = opendir("/mnt/sdcard/frogui/theme-packs");
  if (!dp)
    return;
  struct dirent *e;
  while ((e = readdir(dp)) && theme_pack_count < MAX_THEME_PACKS) {
    if (e->d_name[0] == '.' || e->d_type == DT_REG)
      continue;
    int dup = 0;
    for (int i = 1; i < theme_pack_count; i++)
      if (strcasecmp(theme_pack_files[i], e->d_name) == 0) {
        dup = 1;
        break;
      }
    if (dup)
      continue;
    strncpy(theme_pack_files[theme_pack_count], e->d_name, FONT_STR_MAX - 1);
    theme_pack_files[theme_pack_count][FONT_STR_MAX - 1] = '\0';
    strncpy(theme_pack_disp[theme_pack_count], e->d_name, FONT_STR_MAX - 1);
    theme_pack_disp[theme_pack_count][FONT_STR_MAX - 1] = '\0';
    for (char *p = theme_pack_disp[theme_pack_count]; *p; p++)
      if (*p == '_' || *p == '-')
        *p = ' ';
    theme_pack_count++;
  }
  closedir(dp);
  if (settings_theme_pack_idx < 0 ||
      settings_theme_pack_idx >= theme_pack_count)
    settings_theme_pack_idx = 0;
  /* New installations start with the credited NextUI background set. A
   * saved settings.txt selection still wins when settings are loaded later. */
  if (settings_theme_pack_idx == 0) {
    for (int i = 1; i < theme_pack_count; i++)
      if (strcasecmp(theme_pack_files[i], "Art_Book_NextUI") == 0) {
        settings_theme_pack_idx = i;
        break;
      }
  }
}
static int wall_has_ext(const char *n) {
  const char *d = strrchr(n, '.');
  return d && (strcasecmp(d, ".png") == 0 || strcasecmp(d, ".jpg") == 0 ||
               strcasecmp(d, ".jpeg") == 0 || strcasecmp(d, ".bmp") == 0);
}
void wallpaper_scan(void) {
  wallpaper_count = 0;
  strcpy(wallpaper_files[0], ""); /* slot 0 = None */
  strcpy(wallpaper_disp[0], "None");
  wallpaper_count = 1;
  DIR *dp = opendir("/mnt/sdcard/frogui/wallpapers");
  if (dp) {
    struct dirent *e;
    while ((e = readdir(dp)) && wallpaper_count < MAX_WALL) {
      if (e->d_name[0] == '.' || !wall_has_ext(e->d_name))
        continue;
      int dup = 0;
      for (int i = 1; i < wallpaper_count; i++)
        if (strcasecmp(wallpaper_files[i], e->d_name) == 0) {
          dup = 1;
          break;
        }
      if (dup)
        continue;
      strncpy(wallpaper_files[wallpaper_count], e->d_name, FONT_STR_MAX - 1);
      wallpaper_files[wallpaper_count][FONT_STR_MAX - 1] = '\0';
      strncpy(wallpaper_disp[wallpaper_count], e->d_name, FONT_STR_MAX - 1);
      wallpaper_disp[wallpaper_count][FONT_STR_MAX - 1] = '\0';
      char *dot = strrchr(wallpaper_disp[wallpaper_count], '.');
      if (dot)
        *dot = '\0';
      wallpaper_count++;
    }
    closedir(dp);
  }
  if (settings_wallpaper_idx >= wallpaper_count)
    settings_wallpaper_idx = 0;
}
void font_scan(void) {
  static const char *dirs[] = {
      "/mnt/sdcard/cubegm/fonts",
      "/mnt/sdcard/frogui/fonts",
      "fonts",
  };
  font_count = 0;
  for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
    DIR *dp = opendir(dirs[d]);
    if (!dp)
      continue;
    struct dirent *e;
    while ((e = readdir(dp))) {
      if (e->d_name[0] == '.')
        continue;
      if (font_has_ext(e->d_name))
        font_add(e->d_name);
    }
    closedir(dp);
  }
  /* No fonts on disk: keep the built-in defaults as a safety net. */
  if (font_count == 0) {
    font_add("GamePocket-Regular-ZeroKern.ttf");
    font_add("monogram.ttf");
  }
  /* Sort filenames alphabetically (case-insensitive) for a stable list. */
  for (int i = 1; i < font_count; i++) {
    char key[FONT_STR_MAX];
    strncpy(key, font_files[i], FONT_STR_MAX);
    int j = i - 1;
    while (j >= 0 && strcasecmp(font_files[j], key) > 0) {
      strncpy(font_files[j + 1], font_files[j], FONT_STR_MAX);
      j--;
    }
    strncpy(font_files[j + 1], key, FONT_STR_MAX);
  }
  /* Build display names = filename minus extension. */
  for (int i = 0; i < font_count; i++) {
    strncpy(font_disp[i], font_files[i], FONT_STR_MAX - 1);
    font_disp[i][FONT_STR_MAX - 1] = '\0';
    char *dot = strrchr(font_disp[i], '.');
    if (dot)
      *dot = '\0';
  }
}
/* Pastel themes are complete treatments, not palette-only options.  Pair
 * them with their matching artwork pack whenever the theme is applied. */
void theme_sync_artwork_pack(void) {
  const char *wanted = NULL;
  const char *theme = theme_get_name(settings_theme_idx);
  if (strcmp(theme, "Catppuccin Mocha") == 0)
    wanted = "Catppuccin";
  else if (strcmp(theme, "Aura") == 0)
    wanted = "Aura";
  else if (strcmp(theme, "Canvas Pastel") == 0)
    wanted = "Canvas_Pastel";
  if (!wanted)
    return;
  for (int i = 0; i < theme_pack_count; i++) {
    if (strcasecmp(theme_pack_files[i], wanted) == 0 ||
        strcasecmp(theme_pack_disp[i], wanted) == 0) {
      settings_theme_pack_idx = i;
      return;
    }
  }
}
