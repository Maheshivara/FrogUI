#ifndef GAMELIST_UI_H
#define GAMELIST_UI_H

#include "../app/app_common.h"
#include "../app/app_state.h"
#include "gl_browser.h"

/* ".../roms/<system>" for a path inside a system; false at the roms root and
 * outside ROMS_PATH. */
bool gamelist_root_for(const char *path, char *out, size_t n);
/* gl_open() for the system containing `path` (one stat: call once per scan). */
bool gamelist_dir_active(const char *path, char *root_out, size_t n);
/* Append what the gamelist puts directly in `path` to entries[]. */
void gamelist_fill_dir(const char *root, const char *path);
/* Scraper media folders (media/, images/...) hidden from the system root. */
bool gamelist_is_media_dir(const char *name);
/* Metadata for a ROM path (NULL if none). */
const Game *gamelist_meta(const char *full_path);
/* Gamelist <name> for a file row, or NULL. No stat; one bsearch. */
const char *gamelist_title_for(const char *dir, const char *file);
/* Artwork: gamelist image first, then (if allowed) the legacy .res lookup.
 * kind = ARTWORK_BOXART / ARTWORK_TITLE_SCREEN. 1 on success (tb->data set). */
int game_art_load(const char *full_path, ArtworkKind kind, Thumbnail *tb);
/* Loads an image file (any extension) via render.c into the shared Thumbnail
 * buffer. */
bool load_thumbnail_file(const char *path, Thumbnail *tb);
/* render.c */
int load_thumbnail(const char *base_path, Thumbnail *thumb);

#endif
