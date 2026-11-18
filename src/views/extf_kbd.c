#include "app.h"

/* Extension-entry mode of the on-screen keyboard (review request): reuses
 * the search keyboard (same rows/keys) to let the user type a custom
 * extension for the folder's whitelist. START confirms (normalize lowercase
 * without dot, save via ext_filter_toggle_ext), B cancels back to the
 * picker with the cursor on the Add row. */
bool extf_kbd_active = false;
char extf_kbd_text[EXT_FILTER_EXT_LEN + 2]; /* room for dot typing */
/* Extension-entry keyboard (review request): same keys/rows as the search
 * keyboard. The special row's "search" slot acts as CONFIRM: normalize the
 * typed text to lowercase without the leading dot, add it to the folder's
 * whitelist (persisted), mark the picker dirty and drop back into it.
 * B cancels without saving. Text is capped at the extension length. */
void handle_extf_kbd(void) {
  if (input_was_pressed(FROG_BTN_UP))
    search_kbd_r = (search_kbd_r - 1 + KBD_NROWS) % KBD_NROWS;
  if (input_was_pressed(FROG_BTN_DOWN))
    search_kbd_r = (search_kbd_r + 1) % KBD_NROWS;
  if (input_was_pressed(FROG_BTN_LEFT))
    search_kbd_c--;
  if (input_was_pressed(FROG_BTN_RIGHT))
    search_kbd_c++;
  {
    int rl = kbd_row_len(search_kbd_r);
    if (search_kbd_c < 0)
      search_kbd_c = rl - 1;
    if (search_kbd_c >= rl)
      search_kbd_c = 0;
  }
  int len = (int)strlen(extf_kbd_text);
  if (input_was_pressed(FROG_BTN_A)) {
    if (search_kbd_r == KBD_SPECIAL_ROW) {
      if (search_kbd_c == 0) {        /* space: ignore, no spaces in exts */
      } else if (search_kbd_c == 1) { /* backspace */
        if (len > 0)
          extf_kbd_text[len - 1] = '\0';
      } else { /* CONFIRM */
        if (ext_filter_toggle_ext(extf_folder, extf_kbd_text)) {
          extf_dirty = true;
          ui_toast_show("Extension added");
        } else {
          ui_toast_show("Invalid or duplicate");
        }
        extf_kbd_active = false;
        return;
      }
    } else if (len < (int)sizeof(extf_kbd_text) - 1) {
      extf_kbd_text[len] = KBD_ROWS[search_kbd_r][search_kbd_c];
      extf_kbd_text[len + 1] = '\0';
    }
  }
  if (input_was_pressed(FROG_BTN_Y)) { /* quick backspace */
    if (len > 0)
      extf_kbd_text[len - 1] = '\0';
  }
  if (input_was_pressed(FROG_BTN_START)) { /* START also confirms */
    if (extf_kbd_text[0] && ext_filter_toggle_ext(extf_folder, extf_kbd_text)) {
      extf_dirty = true;
      ui_toast_show("Extension added");
    } else {
      ui_toast_show("Invalid or duplicate");
    }
    extf_kbd_active = false;
    return;
  }
  if (input_was_pressed(FROG_BTN_B)) /* cancel ??? back to picker */
    extf_kbd_active = false;
}
/* Extension-entry keyboard: the search keyboard with an extension header and
 * the typed text shown as ".<text>" (the dot is implied and stripped). */
void render_extf_kbd(void) {
  render_clear_screen(framebuffer);
  render_header(framebuffer, tr("header.add_extension"));

  int y = START_Y;
  char q[96];
  snprintf(q, sizeof(q), ".%s_", extf_kbd_text);
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y, q,
                 COLOR_TEXT);
  y += ITEM_HEIGHT + UI_S(8);

  int cw = UI_S(26), ch = ITEM_HEIGHT;
  for (int r = 0; r < KBD_NROWS; r++) {
    int rl = kbd_row_len(r);
    int ry = y + r * ch;
    for (int c = 0; c < rl; c++) {
      char lbl[8];
      int cx;
      if (r == KBD_SPECIAL_ROW) {
        snprintf(lbl, sizeof(lbl), "%s", KBD_SPECIAL[c]);
        cx = PADDING + c * (cw * 3);
      } else {
        lbl[0] = KBD_ROWS[r][c];
        lbl[1] = '\0';
        cx = PADDING + c * cw;
      }
      if (r == search_kbd_r && c == search_kbd_c)
        render_text_pillbox(framebuffer, cx, ry, lbl, COLOR_SELECT_BG,
                            COLOR_SELECT_TEXT, 7, 0);
      else
        font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, cx, ry, lbl,
                       COLOR_TEXT);
    }
  }
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING,
                 y + KBD_NROWS * ch + UI_S(6), tr("keyboard.ok_cancel"),
                 COLOR_TEXT);
  render_legend(framebuffer, LEGEND_X_NONE, 0, 0);
}
