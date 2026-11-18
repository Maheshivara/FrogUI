#include "../app/app.h"

/* internal helpers */
static void add_core_choice(const char *p);
static int core_choice_cmp(const void *a, const void *b);

CoreChoice core_choices[160];
int core_choice_count = 0;
/* Cores selectable in the per-game/per-folder picker but NOT auto-mapped to any
 * rom folder (no default wiring). Lets users opt into heavier modern cores
 * (FBNeo, mame2003-plus) per game without changing the lightweight defaults. */
static const char *extra_picker_cores[] = {
    CORES_PATH "/fbneo_libretro.so",
    CORES_PATH "/mame2003_plus_libretro.so",
    CORES_PATH "/snes9x2010_libretro.so",
    NULL,
};
static void add_core_choice(const char *p) {
  for (int j = 1; j < core_choice_count; j++)
    if (strcmp(core_choices[j].path, p) == 0)
      return; /* dedup by path */
  if (core_choice_count >=
      (int)(sizeof(core_choices) / sizeof(core_choices[0])))
    return;
  const char *base = strrchr(p, '/');
  base = base ? base + 1 : p;
  char nm[64];
  strncpy(nm, base, sizeof(nm) - 1);
  nm[sizeof(nm) - 1] = '\0';
  char *suf = strstr(nm, "_libretro.so");
  if (suf)
    *suf = '\0';
  strncpy(core_choices[core_choice_count].name, nm, 63);
  core_choices[core_choice_count].name[63] = '\0';
  core_choices[core_choice_count].path = p;
  core_choice_count++;
}
static int core_choice_cmp(const void *a, const void *b) {
  return strcasecmp(((const CoreChoice *)a)->name,
                    ((const CoreChoice *)b)->name);
}
void build_core_choices(void) {
  strcpy(core_choices[0].name, "Default (auto)");
  core_choices[0].path = NULL;
  core_choice_count = 1;
  for (int i = 0; console_mappings[i].console_name; i++)
    add_core_choice(console_mappings[i].core_path);
  for (int i = 0; extra_picker_cores[i]; i++)
    add_core_choice(extra_picker_cores[i]);
  /* Dynamic: every *_libretro.so actually present in the cores dir is
   * selectable too, so a newly dropped-in core shows up in the picker
   * without a TreeFrogUI rebuild. (strdup'd once at init; add_core_choice
   * dedupes against the mapped cores by path.) */
  DIR *d = opendir(CORES_PATH);
  if (d) {
    struct dirent *e;
    while ((e = readdir(d))) {
      const char *n = e->d_name;
      size_t l = strlen(n);
      if (l < 13 || strcmp(n + l - 12, "_libretro.so") != 0)
        continue;
      if (strcmp(n, "frogui_libretro.so") == 0)
        continue; /* the menu itself */
      char full[512];
      snprintf(full, sizeof full, CORES_PATH "/%s", n);
      char *p = strdup(full);
      if (p)
        add_core_choice(p);
    }
    closedir(d);
  }
  /* Alphabetical, keeping "Default (auto)" pinned at index 0. */
  if (core_choice_count > 2)
    qsort(&core_choices[1], core_choice_count - 1, sizeof(CoreChoice),
          core_choice_cmp);
}
int core_choice_index_for_path(const char *path) {
  if (!path)
    return 0;
  for (int i = 1; i < core_choice_count; i++)
    if (strcmp(core_choices[i].path, path) == 0)
      return i;
  return 0;
}
