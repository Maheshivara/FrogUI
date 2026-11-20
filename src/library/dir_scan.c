#include "../app/app.h" // IWYU pragma: keep

#include <limits.h>
#include <stdint.h>

/* internal helpers */
static int dirent_is_dir(const char *parent, const struct dirent *e);
static int direntry_cmp(const void *a, const void *b);
static const char *sys_folder_for(const char *path, char *buf, size_t n);
static int file_is_hidden(const char *name, int imglib, const char *sys);
static int folder_has_games(const char *path, int depth);

/* Grow entries[] to hold at least `need` items WITHOUT losing the current
 * block on failure */
int entries_reserve(int need) {
  if (entries && need <= entry_capacity)
    return 1;
  int nc = entry_capacity > 0 ? entry_capacity : INITIAL_ENTRIES_CAPACITY;
  while (nc < need) {
    if (nc > INT_MAX / 2)
      return 0;
    nc *= 2;
  }
  if ((size_t)nc > SIZE_MAX / sizeof(DirEntry))
    return 0;
  DirEntry *ne = realloc(entries, (size_t)nc * sizeof(DirEntry));
  if (!ne)
    return 0;
  entries = ne;
  entry_capacity = nc;
  return 1;
}

/* Directory test without a stat() syscall where the filesystem already knows
 * the type (FAT/ext fill dirent.d_type). Falls back to stat() only for
 * DT_UNKNOWN / DT_LNK. */
static int dirent_is_dir(const char *parent, const struct dirent *e) {
#ifdef DT_DIR
  if (e->d_type == DT_DIR)
    return 1;
  if (e->d_type != DT_UNKNOWN && e->d_type != DT_LNK)
    return 0; /* DT_REG etc.: known non-directory */
#endif
  char full[MAX_PATH_LEN];
  snprintf(full, sizeof full, "%s/%s", parent, e->d_name);
  struct stat st;
  if (stat(full, &st) != 0)
    return -1;
  return S_ISDIR(st.st_mode) ? 1 : 0;
}

/* dirs first, then case-insensitive alpha. */
static int direntry_cmp(const void *a, const void *b) {
  const DirEntry *x = a, *y = b;
  if (x->is_dir != y->is_dir)
    return y->is_dir - x->is_dir;
  return strcasecmp(x->name, y->name);
}

/* get_filter_folder returns a static buffer that folder_has_games() reuses
 * while recursing, so copy it. Returns NULL when the folder has no filter. */
static const char *sys_folder_for(const char *path, char *buf, size_t n) {
  const char *sf = get_filter_folder(path);
  if (!sf || !sf[0])
    return NULL;
  snprintf(buf, n, "%s", sf);
  return buf;
}

/* True if a (non-directory) entry must not be listed as a game: plain
 * artwork/metadata, or hidden by the per-system extension whitelist (PS1 keeps
 * .cue/.m3u/..., hides companion .bin track blobs). PICO-8 carts are .p8.png:
 * keep them. `imglib` = is_image_library_path(dir), hoisted by the caller since
 * it is constant for the whole directory. The extension is isolated once and
 * passed on ("" when there is none). */
static int file_is_hidden(const char *name, int imglib, const char *sys) {
  const char *ext = strrchr(name, '.');
  size_t nlen = strlen(name);
  int is_p8png = nlen >= 7 && strcasecmp(name + nlen - 7, ".p8.png") == 0;
  if (ext && !is_p8png && !imglib &&
      (strcasecmp(ext, ".csv") == 0 || strcasecmp(ext, ".txt") == 0 ||
       strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".jpg") == 0 ||
       strcasecmp(ext, ".png") == 0))
    return 1;
  return ext_filter_should_hide(sys, ext ? ext + 1 : "") ? 1 : 0;
}

/* True if `path` contains at least one game (any file that isn't
 * artwork/metadata), recursing into subfolders. Used to hide empty rom
 * folders. Bounded depth. */
static int folder_has_games(const char *path, int depth) {
  if (depth > 3)
    return 0;
  DIR *d = opendir(path);
  if (!d)
    return 0;
  char sysbuf[64];
  const char *sys = sys_folder_for(path, sysbuf, sizeof sysbuf);
  int imglib = is_image_library_path(path);
  struct dirent *e;
  int found = 0;
  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] == '.')
      continue;
    int isdir = dirent_is_dir(path, e);
    if (isdir < 0)
      continue;
    if (isdir) {
      char full[MAX_PATH_LEN];
      snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
      if (folder_has_games(full, depth + 1)) {
        found = 1;
        break;
      }
    } else if (!file_is_hidden(e->d_name, imglib, sys)) {
      found = 1;
      break;
    }
  }
  closedir(d);
  return found;
}

void scan_directory(const char *path) {
  /* An unopenable dir is NOT an early return: fall through with an empty
   * listing so the root still gets its Settings/Recents rows. */
  entry_count = 0;
  int at_root = strcmp(path, ROMS_PATH) == 0;

  /* gamelist.xml support. gl_on: this system has a gamelist (the only stat per
   * scan). gl_only: user chose "gamelist only" -> list comes from the XML, no
   * readdir, no file cache. With "gamelist + files" the listing below is the
   * unchanged directory scan; the gamelist only adds names/art/info later. */
  char gl_root[MAX_PATH_LEN];
  int gl_on = !at_root && gamelist_dir_active(path, gl_root, sizeof gl_root);
  int gl_only = gl_on && !settings_gamelist_dir;

  /* Cache fast path: if the dir is unchanged (mtime match), load the sorted
   * listing from disk and skip readdir/stat/empty-check/sort entirely.
   * The ROOT is never cached: its listing depends on the contents of its
   * subfolders (hide-empty), which the root's own mtime does not track. It
   * only has a handful of rows, so rescanning is cheap. */
  struct stat dst;
  int have_mtime = (stat(path, &dst) == 0);
  int use_cache = !at_root && !gl_only && settings_file_cache &&
                  !is_image_library_path(path) && have_mtime;
  int cached = use_cache && cache_load(path, (uint64_t)dst.st_mtime, at_root);

  if (gl_only) {
    gamelist_fill_dir(gl_root, path);
    if (entry_count > 1)
      qsort(entries, entry_count, sizeof(DirEntry), direntry_cmp);
  } else if (!cached) {
    DIR *dir = opendir(path);
    char sysbuf[64];
    const char *sys = sys_folder_for(path, sysbuf, sizeof sysbuf);
    int extf_on = ext_filter_folder_active(sys);
    int imglib = is_image_library_path(path);
    /* Only a COMPLETE scan may be cached: an OOM-truncated or unopenable
     * listing must never be persisted. */
    int complete = (dir != NULL);

    struct dirent *e;
    while (dir && (e = readdir(dir)) != NULL) {
      if (e->d_name[0] == '.')
        continue;
      int isdir = dirent_is_dir(path, e); /* d_type, stat() only if unknown */
      if (isdir < 0)
        continue;
      if (!isdir && file_is_hidden(e->d_name, imglib, sys))
        continue;
      /* Scraper media folders next to gamelist.xml are not games. */
      if (isdir && gl_on && strcmp(path, gl_root) == 0 &&
          gamelist_is_media_dir(e->d_name))
        continue;
      /* Always hide the internal "menu" folder at the root. */
      if (isdir && at_root && strcasecmp(e->d_name, "menu") == 0)
        continue;
      if (isdir && at_root && is_psp_folder(e->d_name))
        continue;
      /* Media libraries live under Apps, not in the Games tab. */
      if (isdir && at_root && is_app_folder_name(e->d_name))
        continue;
      /* Inside a filtered system, per-game folders with no whitelisted
       * file read as broken rows; skip them like hide-empty does. */
      if (isdir && extf_on) {
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (!folder_has_games(full, 0))
          continue;
      }
      /* Hide-empty-folders: at the root, skip rom folders with no games.
       * Only reached for the handful of root folders, never per-ROM. */
      if (isdir && settings_hide_empty && at_root) {
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (!folder_has_games(full, 0))
          continue;
      }
      if (!entries_reserve(entry_count + 1)) {
        complete = 0; /* keep what we have, but don't cache it */
        break;
      }
      strncpy(entries[entry_count].name, e->d_name, 255);
      entries[entry_count].name[255] = '\0';
      entries[entry_count].is_dir = isdir;
      entry_count++;
    }
    if (dir)
      closedir(dir);
    /* Sort: dirs first, then alpha (qsort, n log n). */
    if (entry_count > 1)
      qsort(entries, entry_count, sizeof(DirEntry), direntry_cmp);
    /* Persist the freshly-scanned listing for next time. */
    if (complete && use_cache)
      cache_store(path, (uint64_t)dst.st_mtime, at_root);
  }

  /* Favourites remains part of the Games library. Recents and Settings are
   * real top-level tabs and must never appear as fake system folders. If the
   * row can't be allocated it is simply omitted. */
  if (at_root && favorites_get_count() > 0 &&
      entries_reserve(entry_count + 1)) {
    memmove(&entries[1], &entries[0], (size_t)entry_count * sizeof(DirEntry));
    strncpy(entries[0].name, FAVOURITES_ENTRY_NAME, 255);
    entries[0].name[255] = '\0';
    entries[0].is_dir = 0;
    entry_count++;
  }

  if (!entries)
    entry_count = 0; /* belt: renderer must never walk NULL */
  viewing_recents = false;
  viewing_favourites = false;
  viewing_apps = false;
  viewing_activity = false;
  selected_index = 0;
  scroll_offset = 0;
}
