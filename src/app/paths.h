#ifndef PATHS_H
#define PATHS_H
#include "frogui_config.h"

const char *get_basename(const char *path);
/* First path component under ROMS_PATH ("" at the root, NULL outside it).
 * Returns a STATIC buffer: copy before recursing. */
const char *get_console_folder(const char *path);
/* Key for the extension filter: console folder, else last path component. */
const char *get_filter_folder(const char *path);
/* images/ and photos/ libraries treat jpg/png as content, not artwork. */
int is_image_library_path(const char *path);

#endif
