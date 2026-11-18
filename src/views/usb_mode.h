#ifndef USB_MODE_H
#define USB_MODE_H

#include "../app/app_common.h"

extern bool usb_mode_confirm_active;
extern bool usb_mode_initiated_active;
extern bool usb_mode_wait_for_release;
extern long long usb_mode_initiated_at_ms;
extern uint32_t usb_mode_prev_raw;
bool usb_mode_raw_edge(FrogButton button, uint32_t raw);
void render_usb_confirm(void);
#endif
