#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "app_common.h"



void request_game_launch(const char *core_path, const char *rom_path);
void request_standalone_launch(const char *bin_path,
                                      const char *rom_path);
void request_builtin_launch(const char *bin_path);
void launch_by_path(const char *path);
#endif
