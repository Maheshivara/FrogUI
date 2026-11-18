#ifndef VIEW_APPS_H
#define VIEW_APPS_H

#include "../app/app_common.h"

/* Apps are virtual top-level entries. The on-card folder names remain stable
 * for compatibility, while the Apps tab presents friendly names and hides
 * media folders from the Games library. */
typedef struct {
  const char *key;
  const char *label_key;
  const char *label_en;
  const char *folder_a;
  const char *folder_b;
  const char *bin;
} AppEntry;
extern const AppEntry app_defs[];
const char *app_folder_path(int index, char *out, size_t out_size);
int is_app_folder_name(const char *name);
void scan_apps_tab(void);
const char *app_label(const char *key);
int app_def_count(void);
#endif
