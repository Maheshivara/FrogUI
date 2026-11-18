#ifndef ASSETS_SCAN_H
#define ASSETS_SCAN_H

#include "app_common.h"

/* Fonts are discovered at runtime by scanning the font directories for
 * .ttf/.otf files. font_files[] holds the on-disk filename (persisted in
 * settings + passed to the loader); font_disp[] is the extension-stripped
 * name shown in the menu. */
#define MAX_FONTS 32
#define FONT_STR_MAX 96
/* ---- Wallpaper: one image used across ALL views (index 0 = "None" = the
 * normal per-system background art). Scanned from frogui/wallpapers/.
 * ------------- */
#define MAX_WALL 64
#define WALL_FIT_N 5
/* Optional background packs. Pack 0 is the shipped, flat frogui/ artwork;
 * additional packs live in frogui/theme-packs/<folder>/. Each pack contains
 * the same per-screen filenames (main.jpg, ps.jpg, settings.jpg, ...). */
#define MAX_THEME_PACKS 32
/* System View icon packs are independent from background packs. Pack 0 keeps
 * frogui/system-icons; optional packs live in frogui/icon-packs/<folder>/. */
#define MAX_ICON_PACKS 32
extern char font_files[MAX_FONTS][FONT_STR_MAX];
extern char font_disp[MAX_FONTS][FONT_STR_MAX];
extern int font_count;
extern char wallpaper_files[MAX_WALL][FONT_STR_MAX];
extern char wallpaper_disp[MAX_WALL][FONT_STR_MAX];
extern int wallpaper_count;
extern const char *wallpaper_fit_names[];
extern char theme_pack_files[MAX_THEME_PACKS][FONT_STR_MAX];
extern char theme_pack_disp[MAX_THEME_PACKS][FONT_STR_MAX];
extern int theme_pack_count;
extern char icon_pack_files[MAX_ICON_PACKS][FONT_STR_MAX];
extern char icon_pack_disp[MAX_ICON_PACKS][FONT_STR_MAX];
extern int icon_pack_count;
void icon_pack_scan(void);
void theme_pack_scan(void);
void wallpaper_scan(void);
void font_scan(void);
void theme_sync_artwork_pack(void);
#endif
