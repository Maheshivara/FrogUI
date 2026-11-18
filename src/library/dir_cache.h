#ifndef DIR_CACHE_H
#define DIR_CACHE_H

#include "app_common.h"
#include "settings.h"

/* ---- Folder listing cache (OnionOS-style, defensive) ----------------------
 * Skip the readdir + per-folder empty-check + sort on a repeat visit when the
 * directory hasn't changed. Keyed on the directory's mtime: adding/removing a
 * file bumps mtime, so a stale cache can't survive a real change. Every read is
 * bounds-checked; ANY inconsistency (bad magic, path mismatch, mtime/hide_empty
 * change, short/corrupt file, alloc fail) falls through to a full rescan, which
 * then rewrites the cache. Cache lives in frogui/.cache/, never in the rom
 * dirs, and the whole feature is behind settings_file_cache so it can be turned
 * off. */
#define CACHE_DIR SETTINGS_DIR "/.cache"
#define CACHE_MAGIC                                                            \
  0x33435546u                    /* "FUC3" — bumped when the listing content   \
                                  * changes WITHOUT the dir mtime moving       \
                                  * (e.g. extension-whitelist edits). */
#define CACHE_MAX_ENTRIES 100000 /* sanity cap to reject a corrupt count */

void cache_file_for(const char *path, char *out, size_t n);
void cache_clear(void);
int cache_load(const char *path, uint64_t mtime, int at_root);
void cache_store(const char *path, uint64_t mtime, int at_root);
#endif
