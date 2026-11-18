/* paths.c - roms-root state and path helpers. */
#include <string.h>
#include <strings.h>
#include "paths.h"

char g_roms_path[512] = ROMS_PATH_DEFAULT;

const char *get_basename(const char *path) {
  const char *b = strrchr(path, '/');
  return b ? b + 1 : path;
}

const char *get_console_folder(const char *path) {
  size_t roms_len = strlen(ROMS_PATH);
  if (strncmp(path, ROMS_PATH, roms_len) == 0) {
    const char *sub = path + roms_len;
    if (*sub == '/') {
      sub++;
    }
    static char console[64];
    int i = 0;
    while (sub[i] != '\0' && sub[i] != '/' && i < 63) {
      console[i] = sub[i];
      i++;
    }
    console[i] = '\0';
    return console;
  }
  return NULL;
}

/* Extension filters use the current folder name as their stable key.  ROM
 * paths already resolve through get_console_folder(); app/media folders live
 * elsewhere on the card, so fall back to the final path component. */
const char *get_filter_folder(const char *path) {
  const char *folder = get_console_folder(path);
  if (folder && folder[0])
    return folder;
  const char *base = get_basename(path);
  return (base && base[0]) ? base : NULL;
}

/* JPG/PNG normally mean frontend artwork and stay hidden from ROM listings.
 * Inside the dedicated image libraries they are content, including in nested
 * album/comic folders, so those paths must opt out of the artwork filter. */
int is_image_library_path(const char *path) {
  const char *folder = get_console_folder(path);
  return folder &&
         (!strcasecmp(folder, "images") || !strcasecmp(folder, "photos"));
}
