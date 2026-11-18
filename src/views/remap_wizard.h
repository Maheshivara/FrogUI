#ifndef REMAP_WIZARD_H
#define REMAP_WIZARD_H

#include "../app/app_common.h"

extern bool remap_wizard_active;
extern int remap_step;
extern uint32_t remap_prev_raw;
void handle_remap_wizard(void);
void render_remap_wizard(void);
#endif
