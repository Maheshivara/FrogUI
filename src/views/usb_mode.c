#include "../app/app.h"

/* The UI's idle path polls input at about 1 kHz.  These must not be frame
 * counters: a 240-frame confirmation used to disappear in a fraction of a
 * second, before a deliberately held face button had even passed debounce. */
bool usb_mode_confirm_active = false;
bool usb_mode_initiated_active = false;
bool usb_mode_wait_for_release = false;
long long usb_mode_initiated_at_ms = 0;
uint32_t usb_mode_prev_raw = 0;
/* USB mode is a modal dialog, so it must not inherit browser navigation,
 * per-game bindings, or the face-button filter intended to suppress right-stick
 * drift.  Its A/B controls are read from the physical bits selected in the
 * user's keymap and every other key is deliberately consumed by the modal. */
bool usb_mode_raw_edge(FrogButton button, uint32_t raw) {
  int bit = input_get_raw_bit(button);
  if (bit < 0 || bit > 15)
    return false;
  uint32_t mask = 1u << bit;
  return (raw & mask) && !(usb_mode_prev_raw & mask);
}
void render_usb_confirm(void) {
  render_clear_screen(framebuffer);
  render_header(framebuffer, tr("header.usb_mode"));
  const char *a = usb_mode_initiated_active ? tr("usb.initiated")
                                            : tr("usb.connect_instruction");
  const char *b =
      usb_mode_initiated_active ? tr("usb.back") : tr("usb.connect_back");
  if (!usb_mode_initiated_active)
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                   (SCREEN_WIDTH - font_measure_text(a)) / 2,
                   SCREEN_HEIGHT / 2 - UI_S(18), a, COLOR_TEXT);
  const char *pill =
      usb_mode_initiated_active ? tr("usb.initiated") : tr("usb.ready");
  render_text_pillbox(framebuffer, (SCREEN_WIDTH - font_measure_text(pill)) / 2,
                      SCREEN_HEIGHT / 2 - UI_S(48), pill,
                      /* Keep the selected-theme contrast pair inside the
                       * pillbox; COLOR_TEXT can equal the pill background
                       * on light themes. */
                      COLOR_SELECT_BG, COLOR_SELECT_TEXT, 7, 0);
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                 (SCREEN_WIDTH - font_measure_text(b)) / 2,
                 SCREEN_HEIGHT / 2 + UI_S(12), b, COLOR_TEXT);
  if (usb_mode_initiated_active) {
    const char *warn = tr("usb.transfer_warning");
    font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                   (SCREEN_WIDTH - font_measure_text(warn)) / 2,
                   SCREEN_HEIGHT / 2 + UI_S(42), warn, COLOR_TEXT);
  }
}
