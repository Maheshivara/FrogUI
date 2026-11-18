#ifndef SETTINGS_MENU_H
#define SETTINGS_MENU_H

#include "app_common.h"

/* Data-driven settings menu: rows are HEADERs (non-selectable dividers) or
 * options. Adding, reordering, or grouping = just editing this table; the nav,
 * adjust, and render code all iterate it. TOGGLE/RANGE point at their int;
 * THEME and FONT are special (dynamic option lists); ACTION opens the remap
 * wizard. */
typedef enum {
  RT_HEADER,
  RT_INFO,
  RT_TOGGLE,
  RT_RANGE,
  RT_THEME,
  RT_STYLE,
  RT_FONT,
  RT_WALLPAPER,
  RT_WALLFIT,
  RT_THEME_PACK,
  RT_ICON_PACK,
  RT_ROM_SOURCE,
  RT_OTG_STATUS,
  RT_CACHE_REBUILD,
  RT_ACTION,
  RT_LANGUAGE
} SRowType;
typedef struct {
  SRowType type;
  const char *label;
  int *val;              /* TOGGLE / RANGE target */
  int rmin, rmax, rstep; /* RANGE bounds */
} SRow;
extern const int settings_row_count;
#define SETTINGS_ROW_N settings_row_count
#define SETTINGS_SECTION_N 5
extern const SRow settings_rows[];
extern bool settings_section_open[SETTINGS_SECTION_N];
extern int settings_vis_rows[];
extern int settings_vis_n;
void settings_sections_load(void);
void settings_build_vis_rows(void);
void handle_settings_menu(void);
void render_settings_menu(void);
#endif
