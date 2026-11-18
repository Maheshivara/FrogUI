#include "app.h"

/* internal helpers */
static int restore_games_tab_entries(void);

/* Switch the browser into the recents view (used by the Recents entry and, when
 * "Start in Recents" is on, at startup). */
void enter_recents_view(void) {
  const RecentGame *rg = recent_games_get_list();
  int rc = recent_games_get_count();
  while (rc > entry_capacity) {
    entry_capacity =
        entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
    DirEntry *ne = realloc(entries, entry_capacity * sizeof(DirEntry));
    if (!ne)
      return;
    entries = ne;
  }
  entry_count = 0;
  for (int i = 0; i < rc; i++) {
    strncpy(entries[entry_count].name, rg[i].game_name, 255);
    entries[entry_count].name[255] = '\0';
    entries[entry_count].is_dir = 0;
    entry_count++;
  }
  viewing_recents = true;
  game_switcher_fullscreen = false;
  selected_index = 0;
  scroll_offset = 0;
}
static int games_tab_selected = 0, games_tab_scroll = 0;
static int recents_tab_selected = 0;
int apps_tab_selected = 0;
DirEntry *games_tab_entries = NULL;
int games_tab_entry_count = 0;
void save_games_tab_entries(void) {
  if (!entries || entry_count <= 0) {
    games_tab_entry_count = 0;
    return;
  }
  DirEntry *copy = realloc(games_tab_entries,
                           (size_t)entry_count * sizeof(*games_tab_entries));
  if (!copy)
    return;
  games_tab_entries = copy;
  memcpy(games_tab_entries, entries, (size_t)entry_count * sizeof(*entries));
  games_tab_entry_count = entry_count;
}
static int restore_games_tab_entries(void) {
  if (!games_tab_entries || games_tab_entry_count <= 0)
    return 0;
  if (games_tab_entry_count > entry_capacity) {
    DirEntry *grown =
        realloc(entries, (size_t)games_tab_entry_count * sizeof(*entries));
    if (!grown)
      return 0;
    entries = grown;
    entry_capacity = games_tab_entry_count;
  }
  memcpy(entries, games_tab_entries,
         (size_t)games_tab_entry_count * sizeof(*entries));
  entry_count = games_tab_entry_count;
  viewing_recents = false;
  viewing_favourites = false;
  viewing_search = false;
  return 1;
}
int main_tab_active(void) {
  if (settings_menu_active)
    return MAIN_TAB_SETTINGS;
  if (viewing_recents)
    return MAIN_TAB_RECENTS;
  if (viewing_apps || apps_browsing || viewing_activity)
    return MAIN_TAB_APPS;
  return MAIN_TAB_GAMES;
}
void switch_main_tab(int target) {
  int old = main_tab_active();
  if (target < MAIN_TAB_RECENTS || target > MAIN_TAB_SETTINGS || target == old)
    return;

  ui_transition_start(target > old ? 1 : -1);
  if (old == MAIN_TAB_GAMES) {
    games_tab_selected = selected_index;
    games_tab_scroll = scroll_offset;
    save_games_tab_entries();
  } else if (old == MAIN_TAB_RECENTS) {
    recents_tab_selected = selected_index;
  } else if (old == MAIN_TAB_APPS) {
    apps_tab_selected = selected_index;
  } else {
    settings_save_file();
    settings_menu_active = false;
  }

  if (target == MAIN_TAB_RECENTS) {
    settings_menu_active = false;
    viewing_apps = false;
    apps_browsing = false;
    viewing_activity = false;
    viewing_favourites = false;
    viewing_search = false;
    enter_recents_view();
    if (entry_count > 0) {
      selected_index = recents_tab_selected;
      if (selected_index >= entry_count)
        selected_index = entry_count - 1;
      if (selected_index < 0)
        selected_index = 0;
    }
  } else if (target == MAIN_TAB_GAMES) {
    settings_menu_active = false;
    viewing_recents = false;
    viewing_favourites = false;
    viewing_apps = false;
    apps_browsing = false;
    viewing_activity = false;
    viewing_search = false;
    strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
    current_path[MAX_PATH_LEN - 1] = '\0';
    if (!restore_games_tab_entries())
      scan_directory(current_path);
    if (entry_count > 0) {
      selected_index = games_tab_selected;
      if (selected_index >= entry_count)
        selected_index = entry_count - 1;
      if (selected_index < 0)
        selected_index = 0;
      scroll_offset = games_tab_scroll;
      if (scroll_offset > selected_index)
        scroll_offset = selected_index;
      if (scroll_offset < 0)
        scroll_offset = 0;
    }
  } else if (target == MAIN_TAB_APPS) {
    settings_menu_active = false;
    scan_apps_tab();
    if (entry_count > 0) {
      selected_index = apps_tab_selected;
      if (selected_index >= entry_count)
        selected_index = entry_count - 1;
      if (selected_index < 0)
        selected_index = 0;
    }
  } else {
    viewing_recents = false;
    viewing_favourites = false;
    viewing_apps = false;
    apps_browsing = false;
    viewing_search = false;
    settings_menu_active = true;
    /* Headers are selectable now (they collapse/expand sections); just
     * make sure the cursor sits on a VISIBLE row. */
    settings_build_vis_rows();
    int found = 0;
    for (int i = 0; i < settings_vis_n; i++)
      if (settings_vis_rows[i] == settings_menu_idx) {
        found = 1;
        break;
      }
    if (!found)
      settings_menu_idx = settings_vis_rows[0];
    settings_filter_idx_on_enter = settings_filter_idx;
  }
}
