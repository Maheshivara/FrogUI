#ifndef EXTF_KBD_H
#define EXTF_KBD_H

#include "../app/app_common.h"

extern bool extf_kbd_active;
extern char extf_kbd_text[EXT_FILTER_EXT_LEN + 2];
void handle_extf_kbd(void);
void render_extf_kbd(void);
#endif
