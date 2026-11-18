#include "app.h"

DirEntry *entries = NULL;
int entry_count = 0;
int entry_capacity = 0;
char current_path[MAX_PATH_LEN] = ROMS_PATH_DEFAULT;
int selected_index = 0;
int scroll_offset = 0;
uint16_t *framebuffer = NULL;
bool shutdown_requested = false;
bool viewing_recents = false;
bool viewing_favourites = false;
bool viewing_apps = false;
bool apps_browsing = false;
bool viewing_activity = false;
char apps_root_path[MAX_PATH_LEN] = "";
bool game_switcher_fullscreen = false;
