#ifndef SEARCH_H
#define SEARCH_H

#include "app_common.h"
#include "app_state.h"

typedef struct {
  char name[256];
  char path[MAX_PATH_LEN];
} SearchResult;
#define KBD_SPECIAL_ROW 4
#define KBD_NROWS 5
extern bool search_kbd_active;
extern bool viewing_search;
extern char search_query[64];
extern int search_kbd_r, search_kbd_c;
extern char search_scope[MAX_PATH_LEN];
extern const char *KBD_ROWS[4];
extern const char *KBD_SPECIAL[3];
int kbd_row_len(int r);
void search_launch(int idx);
void handle_search_kbd(void);
void render_search_kbd(void);
#endif
