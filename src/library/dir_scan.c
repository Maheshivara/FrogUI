#include "app.h"

/* internal helpers */
static int dirent_is_dir(const char *parent, const struct dirent *e);
static int direntry_cmp(const void *a, const void *b);
static int folder_has_games(const char *path, int depth);

/* True if `path` contains at least one game (any file that isn't
 * artwork/metadata), recursing into subfolders. Used to hide empty rom folders.
 * Bounded depth. */
/* Directory test without a stat() syscall where the filesystem already knows
 * the type (FAT/ext fill dirent.d_type). Falls back to stat() only for
 * DT_UNKNOWN. stat() per file was the main scan cost on cards with thousands of
 * ROMs. */
static int dirent_is_dir(const char *parent, const struct dirent *e) {
#ifdef DT_DIR
  if (e->d_type == DT_DIR)
    return 1;
  if (e->d_type == DT_REG || e->d_type == DT_LNK) {
    if (e->d_type == DT_REG)
      return 0; /* known file */
  }
  if (e->d_type != DT_UNKNOWN && e->d_type != DT_LNK)
    return 0;
#endif
  char full[MAX_PATH_LEN];
  snprintf(full, sizeof full, "%s/%s", parent, e->d_name);
  struct stat st;
  if (stat(full, &st) != 0)
    return -1; /* unreadable: caller skips */
  return S_ISDIR(st.st_mode) ? 1 : 0;
}
/* dirs first, then case-insensitive alpha. */
static int direntry_cmp(const void *a, const void *b) {
  const DirEntry *x = a, *y = b;
  if (x->is_dir != y->is_dir)
    return y->is_dir - x->is_dir;
  return strcasecmp(x->name, y->name);
}
static int folder_has_games(const char *path, int depth) {
  if (depth > 3)
    return 0;
  DIR *d = opendir(path);
  if (!d)
    return 0;
  struct dirent *e;
  int found = 0;
  /* get_console_folder returns a static buffer; copy it before recursing. */
  char sys_folder[64];
  const char *sf = get_filter_folder(path);
  if (sf) {
    strncpy(sys_folder, sf, sizeof(sys_folder) - 1);
    sys_folder[sizeof(sys_folder) - 1] = '\0';
  } else
    sys_folder[0] = '\0';
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
    } else {
      size_t nlen = strlen(e->d_name);
      int is_p8png =
          nlen >= 7 && strcasecmp(e->d_name + nlen - 7, ".p8.png") == 0;
      const char *ext = strrchr(e->d_name, '.');
      if (ext && !is_p8png && !is_image_library_path(path) &&
          (strcasecmp(ext, ".csv") == 0 || strcasecmp(ext, ".txt") == 0 ||
           strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".png") == 0))
        continue;
      /* Extension already isolated once above (review: normalize/resolve
       * per entry, not per API call) - pass it, or "" when there is none. */
      if (ext_filter_should_hide(sys_folder[0] ? sys_folder : NULL,
                                 ext ? ext + 1 : ""))
        continue;
      found = 1;
      break;
    }
  }
  closedir(d);
  return found;
}
void scan_directory(const char *path) {
  /* An unopenable dir is NOT an early return: fall through with an empty
   * listing so the root still gets its Settings/Recents rows. The old
   * early-return left entries == NULL while the renderer drew the list →
   * NULL deref (SIGBUS addr=0) and a boot crash-loop on cards without roms/. */
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
   * listing from disk and skip readdir/stat/empty-check/sort entirely. */
  struct stat dst;
  int have_mtime = (stat(path, &dst) == 0);
  int cached = !gl_only && settings_file_cache &&
               !is_image_library_path(path) && have_mtime &&
               cache_load(path, (uint64_t)dst.st_mtime, at_root);

  if (gl_only) {
    gamelist_fill_dir(gl_root, path);
    if (entry_count > 1)
      qsort(entries, entry_count, sizeof(DirEntry), direntry_cmp);
  } else if (!cached) {
    DIR *dir = opendir(path);
    struct dirent *e;
    /* Static buffer inside get_console_folder: copy before the loop — the
     * hide-empty branch below calls folder_has_games(), which reuses it. */
    char sys_folder[64];
    const char *sf = get_filter_folder(path);
    if (sf) {
      strncpy(sys_folder, sf, sizeof(sys_folder) - 1);
      sys_folder[sizeof(sys_folder) - 1] = '\0';
    } else
      sys_folder[0] = '\0';
    int extf_on = ext_filter_folder_active(sys_folder[0] ? sys_folder : NULL);
    while (dir && (e = readdir(dir)) != NULL) {
      if (e->d_name[0] == '.')
        continue;
      int isdir = dirent_is_dir(path, e); /* d_type, stat() only if unknown */
      if (isdir < 0)
        continue;
      if (!isdir) {
        const char *ext = strrchr(e->d_name, '.');
        /* PICO-8 carts are .p8.png — keep them; only skip plain .png artwork.
         */
        size_t nlen = strlen(e->d_name);
        int is_p8png =
            nlen >= 7 && strcasecmp(e->d_name + nlen - 7, ".p8.png") == 0;
        if (ext && !is_p8png && !is_image_library_path(path) &&
            (strcasecmp(ext, ".csv") == 0 || strcasecmp(ext, ".txt") == 0 ||
             strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".jpg") == 0 ||
             strcasecmp(ext, ".png") == 0))
          continue;
        /* Per-system extension whitelist (PS1: keep .cue/.m3u/..., hide
         * companion .bin track blobs). Only files are filtered; every
         * other system shows everything as before. The extension was
         * already isolated once above (review: normalize per entry,
         * never per API call) - pass it, or "" when there is none. */
        if (ext_filter_should_hide(sys_folder[0] ? sys_folder : NULL,
                                   ext ? ext + 1 : ""))
          continue;
      }
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
       * file (e.g. a folder left behind after a dump was moved out) read
       * as broken rows; skip them for the same reason as hide-empty. */
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
      if (entry_count >= entry_capacity) {
        entry_capacity =
            entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
        entries = realloc(entries, entry_capacity * sizeof(DirEntry));
        if (!entries) {
          closedir(dir);
          return;
        }
      }
      strncpy(entries[entry_count].name, e->d_name, 255);
      entries[entry_count].name[255] = '\0';
      entries[entry_count].is_dir = isdir;
      entry_count++;
    }
    if (dir)
      closedir(dir);
    /* Sort: dirs first, then alpha. qsort (n log n) — the old n^2 bubble sort
     * made large folders (thousands of ROMs) crawl. */
    if (entry_count > 1)
      qsort(entries, entry_count, sizeof(DirEntry), direntry_cmp);
    /* Persist the freshly-scanned listing for next time. */
    if (settings_file_cache && have_mtime)
      cache_store(path, (uint64_t)dst.st_mtime, at_root);
  }
  /* Cached roots may predate the Apps split; apply the same exclusions after
   * loading a cache so media folders never leak back into Games. */
  if (at_root && entry_count > 0) {
    int write = 0;
    for (int read = 0; read < entry_count; read++) {
      if (entries[read].is_dir &&
          (strcasecmp(entries[read].name, "menu") == 0 ||
           is_psp_folder(entries[read].name) ||
           is_app_folder_name(entries[read].name)))
        continue;
      if (write != read)
        entries[write] = entries[read];
      write++;
    }
    entry_count = write;
  }
  /* Favourites remains part of the Games library. Recents and Settings are
   * real top-level tabs and must never appear as fake system folders. */
  if (strcmp(path, ROMS_PATH) == 0) {
    int has_favs = favorites_get_count() > 0 ? 1 : 0;
    int extras = has_favs;
    while (entry_count + extras > entry_capacity) {
      entry_capacity =
          entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
      entries = realloc(entries, entry_capacity * sizeof(DirEntry));
      if (!entries)
        goto done;
    }
    if (has_favs) {
      memmove(&entries[1], &entries[0], entry_count * sizeof(DirEntry));
      strncpy(entries[0].name, FAVOURITES_ENTRY_NAME, 255);
      entries[0].name[255] = '\0';
      entries[0].is_dir = 0;
      entry_count++;
    }
  }
done:
  if (!entries)
    entry_count = 0; /* belt: renderer must never walk NULL */
  viewing_recents = false;
  viewing_favourites = false;
  viewing_apps = false;
  viewing_activity = false;
  selected_index = 0;
  scroll_offset = 0;
}
