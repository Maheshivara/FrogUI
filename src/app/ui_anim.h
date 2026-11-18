#ifndef UI_ANIM_H
#define UI_ANIM_H

#include "app_common.h"

/* Keep view changes nearly immediate. Three blended frames provide a short,
 * consistent crossfade without making tab navigation wait for a panel slide. */
#define VIEW_TRANSITION_FRAMES 2
/* Measured on the R36SX amp: the speaker line needs several ms live before
 * the amp reproduces, and the SF-class frontend feeds ~100 ms of silence
 * runway before the first real chunk (see the picoarch speaker gate).  The
 * minimum AUDIBLE tick measured on-device was ~60 ms of content; the stock
 * firmware's own navigation blip is ~74 ms.  Anything shorter is swallowed
 * by the ramp-up; anything much longer stops reading as a discrete click.
 * Frames are derived from the core's ACTIVE sample rate so a different
 * audio backend/rate keeps the same 60 ms duration. */
#define UI_TICK_MS 60
/* Cap sized for the longest plausible UI tick (200 ms @ 48 kHz = 9600), not
 * the full second - static buffers are precious on these devices. */
#define UI_TICK_MAX_FRAMES 9600
extern uint16_t *view_transition_old;
extern uint16_t *view_transition_out;
extern int view_transition_frame;
extern char ui_toast_text[96];
extern int ui_toast_frames;
void ui_toast_show(const char *text);
void ui_menu_tick(void);
void ui_transition_start(int direction);
const uint16_t *ui_transition_compose(void);
#endif
