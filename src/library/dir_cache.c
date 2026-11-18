#include "../app/app.h" // IWYU pragma: keep

void cache_file_for(const char *path, char *out, size_t n) {
  /* djb2 hash of the path -> stable filename (exact path re-checked on load,
   * so a hash collision can't return the wrong listing). */
  unsigned long h = 5381;
  for (const unsigned char *p = (const unsigned char *)path; *p; p++)
    h = ((h << 5) + h) ^ *p;
  snprintf(out, n, CACHE_DIR "/%08lx.bin", h & 0xffffffffUL);
}
void cache_clear(void) {
  DIR *dir = opendir(CACHE_DIR);
  if (!dir)
    return;
  struct dirent *e;
  while ((e = readdir(dir)) != NULL) {
    if (e->d_name[0] == '.')
      continue;
    size_t len = strlen(e->d_name);
    if (len < 4 || (strcasecmp(e->d_name + len - 4, ".bin") != 0 &&
                    strcasecmp(e->d_name + len - 4, ".tmp") != 0))
      continue;
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof path, CACHE_DIR "/%s", e->d_name);
    unlink(path);
  }
  closedir(dir);
}
/* Fill entries[]/entry_count from cache if valid for (path, mtime, hide_empty).
 * Returns 1 on a good hit, 0 to force a rescan. */
int cache_load(const char *path, uint64_t mtime, int at_root) {
  char cf[MAX_PATH_LEN];
  cache_file_for(path, cf, sizeof cf);
  FILE *f = fopen(cf, "rb");
  if (!f)
    return 0;
  int ok = 0;
  uint32_t magic = 0, npath = 0, hide = 0, count = 0;
  uint64_t m = 0;
  char pbuf[MAX_PATH_LEN];
  if (fread(&magic, 4, 1, f) != 1 || magic != CACHE_MAGIC)
    goto done;
  if (fread(&m, 8, 1, f) != 1 || m != mtime)
    goto done;
  if (fread(&hide, 4, 1, f) != 1)
    goto done;
  /* hide_empty only changes the ROOT listing; ignore it elsewhere. */
  if (at_root && hide != (uint32_t)(settings_hide_empty ? 1 : 0))
    goto done;
  if (fread(&npath, 4, 1, f) != 1 || npath == 0 || npath >= sizeof pbuf)
    goto done;
  if (fread(pbuf, 1, npath, f) != npath)
    goto done;
  pbuf[npath] = '\0';
  if (strcmp(pbuf, path) != 0)
    goto done; /* collision guard */
  if (fread(&count, 4, 1, f) != 1 || count > CACHE_MAX_ENTRIES)
    goto done;

  entry_count = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint8_t isdir;
    uint16_t nlen;
    if (fread(&isdir, 1, 1, f) != 1)
      goto done;
    if (fread(&nlen, 2, 1, f) != 1 || nlen > 255)
      goto done;
    if (entry_count >= entry_capacity) {
      int nc = entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
      DirEntry *ne = realloc(entries, nc * sizeof(DirEntry));
      if (!ne)
        goto done;
      entries = ne;
      entry_capacity = nc;
    }
    if (fread(entries[entry_count].name, 1, nlen, f) != nlen)
      goto done;
    entries[entry_count].name[nlen] = '\0';
    entries[entry_count].is_dir = isdir ? 1 : 0;
    entry_count++;
  }
  ok = 1;
done:
  fclose(f);
  if (!ok)
    entry_count = 0; /* partial read: discard, caller rescans */
  return ok;
}
void cache_store(const char *path, uint64_t mtime, int at_root) {
  mkdir(CACHE_DIR, 0755);
  char cf[MAX_PATH_LEN], tmp[MAX_PATH_LEN];
  cache_file_for(path, cf, sizeof cf);
  snprintf(tmp, sizeof tmp, "%s.tmp", cf);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return; /* can't write cache: not fatal, just skip */
  uint32_t magic = CACHE_MAGIC,
           hide = at_root ? (settings_hide_empty ? 1 : 0) : 0;
  uint32_t npath = (uint32_t)strlen(path), count = (uint32_t)entry_count;
  int good = 1;
  good &= fwrite(&magic, 4, 1, f) == 1;
  good &= fwrite(&mtime, 8, 1, f) == 1;
  good &= fwrite(&hide, 4, 1, f) == 1;
  good &= fwrite(&npath, 4, 1, f) == 1;
  good &= fwrite(path, 1, npath, f) == npath;
  good &= fwrite(&count, 4, 1, f) == 1;
  for (int i = 0; good && i < entry_count; i++) {
    uint8_t isdir = entries[i].is_dir ? 1 : 0;
    uint16_t nlen = (uint16_t)strlen(entries[i].name);
    good &= fwrite(&isdir, 1, 1, f) == 1;
    good &= fwrite(&nlen, 2, 1, f) == 1;
    good &= fwrite(entries[i].name, 1, nlen, f) == nlen;
  }
  fclose(f);
  if (good)
    rename(tmp, cf); /* atomic swap; a half-write never gets read */
  else
    unlink(tmp);
}
