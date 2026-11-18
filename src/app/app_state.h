#ifndef APP_STATE_H
#define APP_STATE_H

#include "app_common.h"

/* App state */
#define MAX_PATH_LEN 512
#define INITIAL_ENTRIES_CAPACITY 64
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
typedef struct {
  char name[256];
  int is_dir;
} DirEntry;
enum { MAIN_TAB_RECENTS, MAIN_TAB_GAMES, MAIN_TAB_APPS, MAIN_TAB_SETTINGS };
#define SETTINGS_ENTRY_NAME ">> Settings"
#define RECENTS_ENTRY_NAME ">> Recents"
#define FAVOURITES_ENTRY_NAME ">> Favourites"
extern DirEntry *entries;
extern int entry_count;
extern int entry_capacity;
extern char current_path[MAX_PATH_LEN];
extern int selected_index;
extern int scroll_offset;
extern uint16_t *framebuffer;
extern bool shutdown_requested;
extern bool viewing_recents;
extern bool viewing_favourites;
extern bool viewing_apps;
extern bool apps_browsing;
extern bool viewing_activity;
extern char apps_root_path[MAX_PATH_LEN];
extern bool game_switcher_fullscreen;

#endif
