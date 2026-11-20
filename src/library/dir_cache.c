#include "../app/app.h" // IWYU pragma: keep
#include "dir_scan.h"

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

/* Fill entries[]/entry_count from cache if valid for (path, mtime).
 * Returns 1 on a good hit, 0 to force a rescan.
 *
 * The root listing is never cached (see scan_directory): its hide-empty
 * result depends on subfolder contents that the root's mtime doesn't track.
 * File layout is unchanged; the `hide` word is a reserved 0. */
int cache_load(const char *path, uint64_t mtime, int at_root) {
  if (at_root)
    return 0;
  char cf[MAX_PATH_LEN];
  cache_file_for(path, cf, sizeof cf);
  FILE *f = fopen(cf, "rb");
  if (!f)
    return 0;
  /* One big stdio buffer: the many tiny freads below become a few real reads.
   * (Slurping the whole file would double peak memory on big listings.) */
  char iobuf[16384];
  setvbuf(f, iobuf, _IOFBF, sizeof iobuf);

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
  if (fread(&npath, 4, 1, f) != 1 || npath == 0 || npath >= sizeof pbuf)
    goto done;
  if (fread(pbuf, 1, npath, f) != npath)
    goto done;
  pbuf[npath] = '\0';
  if (strcmp(pbuf, path) != 0)
    goto done;
  if (fread(&count, 4, 1, f) != 1 || count > CACHE_MAX_ENTRIES)
    goto done;

  /* count is validated: size the array once instead of doubling per row. A
   * failed reserve leaves the old block intact. */
  entry_count = 0;
  if (!entries_reserve((int)count))
    goto done;

  for (uint32_t i = 0; i < count; i++) {
    uint8_t isdir;
    uint16_t nlen;
    if (fread(&isdir, 1, 1, f) != 1 || isdir > 1)
      goto done;
    if (fread(&nlen, 2, 1, f) != 1 || nlen == 0 ||
        nlen >= sizeof entries[0].name)
      goto done;
    if (fread(entries[entry_count].name, 1, nlen, f) != nlen)
      goto done;
    entries[entry_count].name[nlen] = '\0';
    entries[entry_count].is_dir = isdir;
    entry_count++;
  }
  if (fgetc(f) != EOF)
    goto done;
  ok = 1;
done:
  fclose(f);
  if (!ok)
    entry_count = 0;
  return ok;
}

void cache_store(const char *path, uint64_t mtime, int at_root) {
  if (at_root || entry_count > CACHE_MAX_ENTRIES)
    return;
  mkdir(CACHE_DIR, 0755);
  char cf[MAX_PATH_LEN], tmp[MAX_PATH_LEN];
  cache_file_for(path, cf, sizeof cf);
  snprintf(tmp, sizeof tmp, "%s.tmp", cf);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return; /* can't write cache: not fatal, just skip */
  char iobuf[16384];
  setvbuf(f, iobuf, _IOFBF, sizeof iobuf);

  uint32_t magic = CACHE_MAGIC, hide = 0;
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
    good &= nlen > 0; /* load rejects empty names */
    good &= fwrite(&isdir, 1, 1, f) == 1;
    good &= fwrite(&nlen, 2, 1, f) == 1;
    good &= fwrite(entries[i].name, 1, nlen, f) == nlen;
  }
  /* Buffered writes only fail for real at flush/close (e.g. full SD card):
   * check them BEFORE the rename so a short file is never swapped in. */
  good &= ferror(f) == 0;
  good &= fclose(f) == 0;
  if (good && rename(tmp, cf) == 0)
    return; /* atomic swap; a half-write never gets read */
  unlink(tmp);
}
