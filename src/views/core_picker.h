#ifndef CORE_PICKER_H
#define CORE_PICKER_H

#include "../app/app_common.h"
#include "../app/app_state.h"

/* Rows the picker actually draws: one VISIBLE_ENTRIES slot is used by the
 * subtitle (see render_core_picker), so scroll math must match or the cursor
 * lands on an undrawn row and vanishes. */
#define PICKER_ROWS ((VISIBLE_ENTRIES - 1) < 1 ? 1 : (VISIBLE_ENTRIES - 1))
#define EXTF_POOL_N ((int)(sizeof(extf_pool) / sizeof(extf_pool[0])) - 1)
/* Flat model of a picker row, rebuilt on every open/section toggle. Both
 * section headers are selectable: LEFT/RIGHT (or A) on ">> Cores" /
 * ">> EXTENSIONS" collapses/expands that section so long core lists can be
 * skipped with one press. */
enum {
  PR_CORE_HDR,   /* ">> Cores [-]" section header */
  PR_CORE,       /* "Default (auto)" / one core per row */
  PR_EXT_HDR,    /* ">> EXTENSIONS [-]" section header (filter folders only) */
  PR_EXT_TOGGLE, /* "Filter files: ON/OFF" */
  PR_EXT_BOX,    /* "[x] .cue" */
  PR_EXT_ADD     /* "Add extension..." (opens the on-screen keyboard) */
};
typedef struct {
  int type;
  int core;
  int ext;
} PickerRow;
extern bool core_picker_active;
extern int core_picker_idx;
extern int core_picker_scroll;
extern int core_picker_current;
extern char core_picker_key[MAX_PATH_LEN];
extern char core_picker_title[160];
extern bool extf_in_picker;
extern bool extf_cores_open;
extern bool extf_exts_open;
extern char extf_folder[32];
extern char extf_path[MAX_PATH_LEN];
extern bool extf_inside;
extern bool extf_dirty;
void handle_core_picker(void);
void render_core_picker(void);
#endif
