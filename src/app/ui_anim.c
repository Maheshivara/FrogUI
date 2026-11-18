#include "app.h" // IWYU pragma: keep

uint16_t *view_transition_old = NULL;
uint16_t *view_transition_out = NULL;
int view_transition_frame = VIEW_TRANSITION_FRAMES + 1;
char ui_toast_text[96] = "";
int ui_toast_frames = 0;
void ui_toast_show(const char *text) {
  if (!text)
    return;
  strncpy(ui_toast_text, text, sizeof ui_toast_text - 1);
  ui_toast_text[sizeof ui_toast_text - 1] = '\0';
  ui_toast_frames = 90; /* about 1.5 seconds at the presented 60 fps */
}
void ui_menu_tick(void) {
  if (!settings_menu_sounds)
    return;
  static int16_t tick[UI_TICK_MAX_FRAMES * 2];
  static int frames = 0;
  static int ready = 0;
  if (!ready || frames == 0) {
    frames = (int)((double)SAMPLE_RATE * UI_TICK_MS / 1000.0 + 0.5);
    if (frames < 16)
      frames = 16; /* never degenerate */
    if (frames > UI_TICK_MAX_FRAMES)
      frames = UI_TICK_MAX_FRAMES;
    for (int i = 0; i < frames; i++) {
      int16_t s = ((i / 25) & 1) ? 900 : -900;
      int fade = (i < 200) ? i : (frames - i);
      if (fade > 2000)
        fade = 2000;
      if (fade < 1)
        fade = 1;
      s = (int16_t)((int)s * fade / 2000);
      tick[i * 2] = tick[i * 2 + 1] = s;
    }
    ready = 1;
  }
  if (audio_batch_cb)
    audio_batch_cb(tick, frames);
  else if (audio_cb)
    for (int i = 0; i < frames; i++)
      audio_cb(tick[i * 2], tick[i * 2 + 1]);
}
void ui_transition_start(int direction) {
  (void)direction;
  if (!settings_anim || !framebuffer)
    return;
  size_t bytes = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
  if (!view_transition_old)
    view_transition_old = malloc(bytes);
  if (!view_transition_out)
    view_transition_out = malloc(bytes);
  if (!view_transition_old || !view_transition_out)
    return;
  memcpy(view_transition_old, framebuffer, bytes);
  view_transition_frame = 0;
}
const uint16_t *ui_transition_compose(void) {
  if (!settings_anim || view_transition_frame > VIEW_TRANSITION_FRAMES ||
      !view_transition_old || !view_transition_out)
    return framebuffer;

  /* Blend at 25%, 50%, then 75% new view. Red/blue can be processed as one
   * packed field; green is separate. The denominator is four, so the hot
   * loop needs only multiplies, masks and a shift on the MIPS devices. */
  unsigned new_weight = (unsigned)view_transition_frame + 1u;
  unsigned old_weight = 4u - new_weight;
  size_t pixels = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT;
  for (size_t i = 0; i < pixels; i++) {
    uint32_t old = view_transition_old[i];
    uint32_t now = framebuffer[i];
    uint32_t rb =
        (((old & 0xf81fu) * old_weight + (now & 0xf81fu) * new_weight) >> 2) &
        0xf81fu;
    uint32_t g =
        (((old & 0x07e0u) * old_weight + (now & 0x07e0u) * new_weight) >> 2) &
        0x07e0u;
    view_transition_out[i] = (uint16_t)(rb | g);
  }
  view_transition_frame++;
  return view_transition_out;
}
