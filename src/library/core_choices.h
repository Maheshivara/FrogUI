#ifndef CORE_CHOICES_H
#define CORE_CHOICES_H

#include "../app/app_common.h"

/* --- Available cores for the per-game / per-folder core picker ---
 * Deduped list built from console_mappings. Index 0 = "Default (auto)"
 * which clears any override and falls back to folder/extension mapping. */
typedef struct {
  char name[64];
  const char *path;
} CoreChoice;
extern CoreChoice core_choices[160];
extern int core_choice_count;
void build_core_choices(void);
int core_choice_index_for_path(const char *path);
#endif
