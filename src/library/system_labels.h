#ifndef SYSTEM_LABELS_H
#define SYSTEM_LABELS_H

#include "app_common.h"

typedef struct {
  const char *folder;
  const char *label_en;
  char label[96];
} SystemLabel;
#define BANNER_DIR SDCARD_BASE "/frogui"

void system_labels_refresh(void);
const char *system_display_name(const char *folder);
void load_banner_for_view(const char *path, bool is_recents,
                                 bool is_favourites);
#endif
