#include "core_picker.h"
#include "../app/app.h" // IWYU pragma: keep

/* internal helpers */
static void picker_rows_build(void);
static void picker_cursor_to(int type);

/* Per-game / per-folder core picker overlay (opened with SELECT) */
bool core_picker_active = false;
int core_picker_idx = 0;
int core_picker_scroll = 0;
int core_picker_current = 0;        /* index of the ACTIVE core (marked ">>") */
char core_picker_key[MAX_PATH_LEN]; /* ROM path (per-game) or folder path */
char core_picker_title[160];        /* shown under header */
/* Extension-filter section of the picker (PS1 family). The picker shows two
 * collapsible sections, each with a selectable ">> " header:
 *   ">> Cores"              core rows (Default (auto), each core)
 *   ">> EXTENSIONS"         "Filter files: ON" master toggle + "[x] .cue"
 * LEFT/RIGHT (or A) on a header collapses/expands its section so long lists
 * can be skipped quickly. The active core keeps a "->" marker — ">> " is
 * reserved for section headers. */
bool extf_in_picker = false;
bool extf_cores_open = true;  /* ">> Cores" section expanded */
bool extf_exts_open = true;   /* ">> EXTENSIONS" section expanded */
char extf_folder[32] = "";    /* system folder the picker filters */
char extf_path[MAX_PATH_LEN]; /* full SD path of that folder */
bool extf_inside = false;     /* picker opened on a file inside it */
bool extf_dirty = false;      /* a filter edit needs a rescan */
/* Candidate extensions offered for whitelists. PS1 ships games as .cue +
 * .bin track blobs (and .chd/.pbp/... dumps): whitelisting the loadable
 * index formats makes each game appear once. */
static const char *extf_pool[] = {
    /* Common ROM/media extensions.  The picker is available for every
     * browsable folder; users can add anything else through the keyboard. */
    "zip", "7z",  "cue", "bin", "m3u",  "pbp", "iso",    "chd", "img",
    "mdf", "sbi", "ccd", "ecm", "gba",  "gb",  "gbc",    "nes", "smc",
    "sfc", "md",  "gen", "sms", "gg",   "pce", "a26",    "a78", "lnx",
    "ws",  "wsc", "nds", "cia", "love", "p8",  "p8.png", NULL};
static PickerRow picker_rows_buf[256];
static int picker_rows_n = 0;
static void picker_rows_build(void) {
  int n = 0;
  picker_rows_buf[n].type = PR_CORE_HDR;
  picker_rows_buf[n].core = -1;
  picker_rows_buf[n].ext = -1;
  n++;
  if (extf_cores_open)
    for (int i = 0; i < core_choice_count; i++) {
      if (n >= (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0])))
        break;
      picker_rows_buf[n].type = PR_CORE;
      picker_rows_buf[n].core = i;
      picker_rows_buf[n].ext = -1;
      n++;
    }
  if (extf_in_picker) {
    if (n >= (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0]))) {
      picker_rows_n = n;
      return;
    }
    picker_rows_buf[n].type = PR_EXT_HDR;
    picker_rows_buf[n].core = -1;
    picker_rows_buf[n].ext = -1;
    n++;
    if (extf_exts_open) {
      if (n >= (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0]))) {
        picker_rows_n = n;
        return;
      }
      picker_rows_buf[n].type = PR_EXT_TOGGLE;
      picker_rows_buf[n].core = -1;
      picker_rows_buf[n].ext = -1;
      n++;
      for (int e = 0; e < EXTF_POOL_N; e++) {
        if (n >= (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0])))
          break;
        picker_rows_buf[n].type = PR_EXT_BOX;
        picker_rows_buf[n].core = -1;
        picker_rows_buf[n].ext = e;
        n++;
      }
      /* Custom extensions stored for this folder come after the pool. */
      int custom_n = ext_filter_ext_count(extf_folder);
      for (int e = 0; e < custom_n; e++) {
        if (n >= (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0])))
          break;
        picker_rows_buf[n].type = PR_EXT_BOX;
        picker_rows_buf[n].core = -1;
        picker_rows_buf[n].ext = EXTF_POOL_N + e; /* stored-list index */
        n++;
      }
      if (n < (int)(sizeof(picker_rows_buf) / sizeof(picker_rows_buf[0]))) {
        picker_rows_buf[n].type = PR_EXT_ADD;
        picker_rows_buf[n].core = -1;
        picker_rows_buf[n].ext = -1;
        n++;
      }
    }
  }
  picker_rows_n = n;
}
/* Move the cursor onto the (first) row of the given type after a rebuild. */
static void picker_cursor_to(int type) {
  for (int i = 0; i < picker_rows_n; i++)
    if (picker_rows_buf[i].type == type) {
      core_picker_idx = i;
      break;
    }
}
void handle_core_picker(void) {
  picker_rows_build();
  int total = picker_rows_n;
  if (input_was_pressed(FROG_BTN_UP)) {
    core_picker_idx = (core_picker_idx - 1 + total) % total;
  }
  if (input_was_pressed(FROG_BTN_DOWN)) {
    core_picker_idx = (core_picker_idx + 1) % total;
  }
  if (input_was_pressed(FROG_BTN_LEFT)) {
    int t = picker_rows_buf[core_picker_idx].type;
    if (t == PR_CORE_HDR || t == PR_EXT_HDR) {
      if (t == PR_CORE_HDR)
        extf_cores_open = !extf_cores_open;
      else
        extf_exts_open = !extf_exts_open;
      picker_rows_build();
      total = picker_rows_n;
      picker_cursor_to(t); /* stay on the header we just flipped */
    } else {
      core_picker_idx -= PICKER_ROWS;
      if (core_picker_idx < 0)
        core_picker_idx = 0;
    }
  }
  if (input_was_pressed(FROG_BTN_RIGHT)) {
    int t = picker_rows_buf[core_picker_idx].type;
    if (t == PR_CORE_HDR || t == PR_EXT_HDR) {
      if (t == PR_CORE_HDR)
        extf_cores_open = true;
      else
        extf_exts_open = true;
      picker_rows_build();
      total = picker_rows_n;
    } else {
      core_picker_idx += PICKER_ROWS;
      if (core_picker_idx >= total)
        core_picker_idx = total - 1;
    }
  }
  if (core_picker_idx < core_picker_scroll)
    core_picker_scroll = core_picker_idx;
  if (core_picker_idx >= core_picker_scroll + PICKER_ROWS)
    core_picker_scroll = core_picker_idx - PICKER_ROWS + 1;
  if (input_was_pressed(FROG_BTN_A)) {
    int t = picker_rows_buf[core_picker_idx].type;
    if (t == PR_CORE_HDR || t == PR_EXT_HDR) {
      /* A on a header flips its section (same as LEFT). */
      if (t == PR_CORE_HDR)
        extf_cores_open = !extf_cores_open;
      else
        extf_exts_open = !extf_exts_open;
      picker_rows_build();
      total = picker_rows_n;
      picker_cursor_to(t);
    } else if (t == PR_EXT_TOGGLE) {
      ext_filter_set_enabled(extf_folder, !ext_filter_get_enabled(extf_folder));
      extf_dirty = true;
    } else if (t == PR_EXT_BOX) {
      int ei = picker_rows_buf[core_picker_idx].ext;
      if (ei < EXTF_POOL_N)
        ext_filter_toggle_ext(extf_folder, extf_pool[ei]);
      else {
        /* stored (custom) extension: resolve by index */
        char ext[EXT_FILTER_EXT_LEN + 1];
        if (ext_filter_get_ext_at(extf_folder, ei - EXTF_POOL_N, ext,
                                  sizeof ext))
          ext_filter_toggle_ext(extf_folder, ext); /* toggling removes it */
      }
      extf_dirty = true;
    } else if (t == PR_EXT_ADD) {
      /* Open the on-screen keyboard in extension-entry mode. */
      extf_kbd_active = true;
      extf_kbd_text[0] = '\0';
      search_kbd_r = 0;
      search_kbd_c = 0;
    } else if (t == PR_CORE) {
      int cidx = picker_rows_buf[core_picker_idx].core;
      core_override_set(core_picker_key, core_choices[cidx].path);
      ui_toast_show(cidx == 0 ? "Core override cleared"
                              : "Core saved for this item");
      core_picker_active = false;
    }
  }
  if (input_was_pressed(FROG_BTN_B)) {
    core_picker_active = false;
  }
  /* Leaving the picker (either button) after a filter edit: the dir mtime
   * didn't move, so drop the affected cached listings by hand and rescan
   * if we are sitting inside the filtered folder. The current view's cache
   * is dropped too: with hide-empty on, a root listing may need the folder
   * back (or gone) even when the folder's own mtime is unchanged. */
  if (!core_picker_active && extf_in_picker && extf_dirty) {
    char cf[MAX_PATH_LEN];
    cache_file_for(extf_path, cf, sizeof cf);
    unlink(cf);
    cache_file_for(current_path, cf, sizeof cf);
    unlink(cf);
    if (extf_inside)
      scan_directory(current_path);
    extf_dirty = false;
  }
}
void render_core_picker(void) {
  if (banner_is_loaded())
    banner_render(framebuffer);
  else
    render_clear_screen(framebuffer);
  render_header(framebuffer, tr("header.select_core"));

  int y = START_Y;
  /* subtitle: which game/folder we're overriding */
  font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y,
                 core_picker_title, COLOR_TEXT);
  y += ITEM_HEIGHT;

  /* Draw the window of rows [scroll, scroll+PICKER_ROWS). Section headers
   * draw like Settings dividers but stay selectable: the pillbox cursor
   * lands on them, LEFT/RIGHT/A collapse or expand their section. */
  int total = picker_rows_n;
  for (int i = 0; i < PICKER_ROWS; i++) {
    int idx = core_picker_scroll + i;
    if (idx >= total)
      break;
    const PickerRow *r = &picker_rows_buf[idx];
    char line[96];
    switch (r->type) {
    case PR_CORE_HDR:
      snprintf(line, sizeof(line), ">> %s", tr("picker.cores"));
      break;
    case PR_EXT_HDR:
      snprintf(line, sizeof(line), ">> %s", tr("picker.extensions"));
      break;
    case PR_EXT_TOGGLE:
      snprintf(
          line, sizeof(line), "   %s: %s", tr("picker.filter_files"),
          tr(ext_filter_get_enabled(extf_folder) ? "value.on" : "value.off"));
      break;
    case PR_EXT_BOX: {
      char ext[EXT_FILTER_EXT_LEN + 1];
      const char *extp;
      if (r->ext < EXTF_POOL_N)
        extp = extf_pool[r->ext];
      else if (ext_filter_get_ext_at(extf_folder, r->ext - EXTF_POOL_N, ext,
                                     sizeof ext))
        extp = ext;
      else
        extp = "?";
      snprintf(line, sizeof(line), "   [%c] .%s",
               ext_filter_has_ext(extf_folder, extp) ? 'x' : ' ', extp);
      break;
    }
    case PR_EXT_ADD:
      snprintf(line, sizeof(line), "   %s", tr("picker.add_extension"));
      break;
    case PR_CORE:
      /* "->" marks the currently-active core; ">> " is reserved for
       * the section headers above. */
      snprintf(line, sizeof(line), "%s%s",
               r->core == core_picker_current ? "-> " : "   ",
               core_choices[r->core].name);
      break;
    default:
      line[0] = '\0';
      break;
    }
    int ry = y + i * ITEM_HEIGHT;
    if (r->type == PR_CORE_HDR || r->type == PR_EXT_HDR) {
      /* headers: accent color; pillbox when the cursor is on them */
      if (idx == core_picker_idx)
        render_text_pillbox(framebuffer, PADDING, ry, line, COLOR_SELECT_BG,
                            COLOR_SELECT_TEXT, 7, 0);
      else
        font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, ry,
                       line, COLOR_SELECT_BG);
    } else if (idx == core_picker_idx) {
      render_text_pillbox(framebuffer, PADDING, ry, line, COLOR_SELECT_BG,
                          COLOR_SELECT_TEXT, 7, 0);
    } else {
      font_draw_text(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, ry,
                     line, COLOR_TEXT);
    }
  }
  render_scroll_indicator(framebuffer, total, core_picker_idx, PICKER_ROWS);
  render_legend(framebuffer, LEGEND_X_NONE, 0, 0);
}
