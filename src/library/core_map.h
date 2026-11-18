#ifndef CORE_MAP_H
#define CORE_MAP_H
#include <stdbool.h>
#include "frogui_config.h"

typedef struct {
  const char *console_name;
  const char *core_path;
} ConsoleMapping;
/* NULL-terminated table; also walked by the core picker. */
extern const ConsoleMapping console_mappings[];

/* Core for a roms/<folder> name (case-insensitive), or NULL. */
const char *get_core_for_folder(const char *folder);
/* Fallback: core by file extension (".gb" style, with dot), or NULL. */
const char *get_core_for_extension(const char *filename);

bool is_ps1_folder(const char *folder);
bool is_pico286_folder(const char *folder);
bool is_lgpt_folder(const char *folder);
bool is_psp_folder(const char *folder);
/* True if `name` is a standalone binary (run directly, not a libretro core). */
bool is_standalone_bin(const char *name);

#endif
