/*
 * gl_browser.h - glue between gamelist.c (XML parser) and the ROM browser.
 *
 * One gamelist.xml per system folder (<roms>/<system>/gamelist.xml). The
 * parsed list is kept in RAM for ONE system at a time and indexed by the
 * game's path relative to the system folder, so lookups from the list view,
 * the box-art panel, Recents and Favourites are all O(log n) bsearches.
 */
#ifndef GL_BROWSER_H
#define GL_BROWSER_H

#include "gamelist.h"
#include <stdbool.h>
#include <stddef.h>

/* (Re)load <sys_root>/gamelist.xml if it changed on disk (mtime+size).
 * Does a stat(): call it once per directory scan, NOT per frame.
 * Returns true when the system has a usable gamelist (>= 1 game). */
bool gl_open(const char *sys_root);

/* Metadata for a ROM, or NULL. Reuses the loaded list when sys_root matches
 * (no stat, safe to call per row/frame); loads it otherwise. A system without
 * a gamelist is remembered, so repeated calls stay cheap. */
const Game *gl_lookup(const char *sys_root, const char *full_path);

/* Field value, or "" when the field is absent / still at its default
 * ("unknown", "0.000000", "not-a-date-time"...). Never NULL. */
const char *gl_get(const Game *g, const char *key);

/* Absolute path of an image field ("image", "thumbnail"), resolved against
 * sys_root. Returns false if the field is empty. Does not check existence. */
bool gl_image_path(const char *sys_root, const Game *g, const char *key,
                   char *out, size_t n);

/* Enumerate what the gamelist places directly inside dir_path (which must be
 * sys_root or a subfolder of it): games as files, and the first path
 * component of deeper games as (deduplicated) directories. */
typedef void (*gl_entry_cb)(const char *name, int is_dir, void *ud);
void gl_list_dir(const char *sys_root, const char *dir_path, gl_entry_cb cb,
                 void *ud);

/* True if the system has a usable gamelist (cheap, no stat when cached). */
bool gl_has_list(const char *sys_root);

/* Drop the cached list (e.g. after the ROM source changes). */
void gl_close(void);

#endif
