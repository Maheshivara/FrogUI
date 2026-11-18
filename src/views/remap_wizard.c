#include "app.h"

/* internal helpers */
static int remap_wizard_count(void);

bool remap_wizard_active = false;
int remap_step = 0;
uint32_t remap_prev_raw = 0;
/* Wizard covers every button except FN, which is only probed on devices that
 * have the physical button (device detection, see input_fn_available). FN is
 * the last enum entry, so on FN-less devices the wizard finishes right after
 * SELECT and never asks for a button that isn't there. */
static int remap_wizard_count(void) {
  return FROG_BTN_COUNT - (input_fn_available() ? 0 : 1);
}
void handle_remap_wizard(void) {
  uint32_t raw = input_get_raw_state();
  uint32_t risen = raw & ~remap_prev_raw;
  remap_prev_raw = raw;
  int b_bit = input_get_raw_bit(FROG_BTN_B);
  /* The wizard may be entered with an old or manually-edited keymap. A bad
   * B binding must not turn the shift below into undefined behaviour. */
  bool skip =
      b_bit >= 0 && b_bit <= FROG_RAW_MAX_BIT && ((risen & (1u << b_bit)) != 0);
  int pressed_bit = -1;
  if (!skip) {
    for (int bit = 0; bit < FROG_RAW_BIT_COUNT; bit++) {
      if ((risen >> bit) & 1) {
        pressed_bit = bit;
        break;
      }
    }
  }
  if (skip || pressed_bit >= 0) {
    if (!skip)
      input_set_raw_bit((FrogButton)remap_step, pressed_bit);
    {
      char msg[96];
      snprintf(msg, sizeof(msg), "remap: step=%d button=%s raw=%d skip=%d",
               remap_step, input_btn_name((FrogButton)remap_step), pressed_bit,
               skip ? 1 : 0);
      dbg(msg);
    }
    remap_step++;
    if (remap_step >= remap_wizard_count()) {
      remap_wizard_active = false;
      if (input_save_remap(KEYMAP_FILE) == 0)
        ui_toast_show("Button mapping saved");
      else
        ui_toast_show("Could not save button mapping");
      /* Button Mapping belongs to Settings. Returning to its caller
       * avoids dropping the user into a partly stale Games/System view
       * after the final bind, which looked like empty icon tiles until
       * the next full UI reload. */
      settings_menu_active = true;
      settings_build_vis_rows();
      {
        char msg[96];
        snprintf(
            msg, sizeof(msg),
            "remap: complete saved=%d return=settings style=%d icon_pack=%d",
            access(KEYMAP_FILE, R_OK) == 0, settings_style,
            settings_icon_pack_idx);
        dbg(msg);
      }
    }
  }
}
void render_remap_wizard(void) {
  if (banner_is_loaded())
    banner_render(framebuffer);
  else
    render_clear_screen(framebuffer);
  render_header(framebuffer, tr("header.button_mapping"));

  char line[128];
  int y = START_Y;
  snprintf(line, sizeof(line), "Press  %s  (%d / %d)",
           input_btn_name((FrogButton)remap_step), remap_step + 1,
           remap_wizard_count());
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y, line,
                 COLOR_TEXT);

  y += ITEM_HEIGHT;
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y,
                 tr("remap.skip"), COLOR_TEXT);
}
