#ifndef SYSTEM_VIEW_H
#define SYSTEM_VIEW_H

#include "app_common.h"

#define SYSTEM_CAROUSEL_FRAMES 12
/* The system grid uses small cached icons independently from background packs.
 * Optional artwork comes from icon-packs/<pack>/<rom-folder>.png; the shared
 * frogui/system-icons folder is the fallback. A short LRU keeps SD reads out of
 * normal navigation without retaining every system logo on low-memory devices.
 */
#define SYSTEM_ICON_CACHE_N 12
#define SYSTEM_ICON_MAX_W 160
#define SYSTEM_ICON_MAX_H 160
typedef struct {
  char path[512];
  uint16_t *pixels;
  uint8_t *alpha;
  int width, height;
  uint8_t bright;
  unsigned used;
} SystemIcon;
#define SYSTEM_ICON_MISSING_N 32
extern int system_carousel_frame;
extern float system_carousel_start_offset;
extern float system_carousel_visible_offset;
extern SystemIcon system_icons[SYSTEM_ICON_CACHE_N];
extern int system_icon_pack;
extern int system_icon_missing_count;
bool horizontal_system_view(void);
bool icon_system_view(void);
bool system_carousel_is_animating(void);
void system_carousel_start(int direction);
void render_system_carousel(uint16_t *fb);
void render_system_grid(uint16_t *fb);
#endif
