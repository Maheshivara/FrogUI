#ifndef VIEW_ACTIVITY_H
#define VIEW_ACTIVITY_H

#include "../app/app_common.h"
#include "../app/app_state.h"

extern char activity_paths[128][MAX_PATH_LEN];
extern long activity_seconds[128];
extern long activity_runs[128];
extern time_t activity_last[128];
extern int activity_count;
void enter_activity_view(void);
void render_activity_page(uint16_t *fb);
#endif
