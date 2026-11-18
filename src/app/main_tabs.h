#ifndef MAIN_TABS_H
#define MAIN_TABS_H

#include "app_common.h"
#include "app_state.h"


extern int apps_tab_selected;
extern DirEntry *games_tab_entries;
extern int games_tab_entry_count;
void enter_recents_view(void);
void save_games_tab_entries(void);
int main_tab_active(void);
void switch_main_tab(int target);
#endif
