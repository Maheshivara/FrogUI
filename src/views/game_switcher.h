#ifndef GAME_SWITCHER_H
#define GAME_SWITCHER_H

#include "app_common.h"



long playtime_lookup(const char *path);
void switcher_blit565(uint16_t *fb, const uint16_t *src,
                             const uint8_t *alpha, int sw, int sh, int bx,
                             int by, int bw, int bh);
void render_game_switcher(uint16_t *framebuffer);
#endif
