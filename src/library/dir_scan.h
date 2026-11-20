#ifndef DIR_SCAN_H
#define DIR_SCAN_H

#include "../app/app_common.h"

void scan_directory(const char *path);
int entries_reserve(int need);
#endif
