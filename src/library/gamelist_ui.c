#include "../app/app.h" // IWYU pragma: keep
#include "../vendor/stb_image.h"

bool gamelist_root_for(const char *path, char *out, size_t n) {
  if (!path || strcmp(path, ROMS_PATH) == 0)
    return false;
  const char *c = get_console_folder(path); /* static buffer: use now */
  if (!c || !c[0])
    return false;
  snprintf(out, n, "%s/%s", ROMS_PATH, c);
  return true;
}

bool gamelist_dir_active(const char *path, char *root_out, size_t n) {
  return gamelist_root_for(path, root_out, n) && gl_open(root_out);
}

static void fill_cb(const char *name, int is_dir, void *ud) {
  (void)ud;
  if (entry_count >= entry_capacity) {
    int nc = entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
    DirEntry *ne = realloc(entries, (size_t)nc * sizeof(DirEntry));
    if (!ne)
      return;
    entries = ne;
    entry_capacity = nc;
  }
  strncpy(entries[entry_count].name, name, 255);
  entries[entry_count].name[255] = '\0';
  entries[entry_count].is_dir = is_dir;
  entry_count++;
}

void gamelist_fill_dir(const char *root, const char *path) {
  gl_list_dir(root, path, fill_cb, NULL);
}

bool gamelist_is_media_dir(const char *n) {
  static const char *skip[] = {"media",
                               "images",
                               "videos",
                               "manuals",
                               "downloaded_images",
                               "downloaded_media",
                               NULL};
  for (int i = 0; skip[i]; i++)
    if (strcasecmp(n, skip[i]) == 0)
      return true;
  return false;
}

const Game *gamelist_meta(const char *full_path) {
  char root[MAX_PATH_LEN];
  if (!gamelist_root_for(full_path, root, sizeof root))
    return NULL;
  return gl_lookup(root, full_path);
}

const char *gamelist_title_for(const char *dir, const char *file) {
  char full[MAX_PATH_LEN];
  snprintf(full, sizeof full, "%s/%s", dir, file);
  const char *n = gl_get(gamelist_meta(full), "name");
  return n[0] ? n : NULL;
}

/* ---- image loading ------------------------------------------------------ */

/* Gamelist images go through render.c's own loader so they behave exactly like
 * .res artwork: decoded into render.c's shared static buffer (capped 250x250,
 * alpha kept), nothing to free, free_thumbnail() is a no-op reset.
 * load_thumbnail() takes a path WITHOUT extension and probes
 * .png/.jpg/.jpeg/.bmp (then .rgb565), so strip a known image extension. */
bool load_thumbnail_file(const char *path, Thumbnail *tb) {
  char base[MAX_PATH_LEN];
  if (!path || !tb ||
      snprintf(base, sizeof base, "%s", path) >= (int)sizeof base)
    return false;
  char *slash = strrchr(base, '/');
  char *dot = strrchr(base, '.');
  if (dot && (!slash || dot > slash) &&
      (!strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") ||
       !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".bmp") ||
       !strcasecmp(dot, ".rgb565")))
    *dot = '\0';
  return load_thumbnail(base, tb) && tb->data;
}

int game_art_load(const char *full_path, ArtworkKind kind, Thumbnail *tb) {
  char root[MAX_PATH_LEN], img[MAX_PATH_LEN];
  bool have_root = gamelist_root_for(full_path, root, sizeof root);
  const Game *g = have_root ? gl_lookup(root, full_path) : NULL;
  if (g) {
    /* ES convention: thumbnail = box art, image = screenshot/title. */
    const char *k1 = "image";
    const char *k2 = "thumbnail";
    if ((gl_image_path(root, g, k1, img, sizeof img) &&
         load_thumbnail_file(img, tb) && tb->data) ||
        (gl_image_path(root, g, k2, img, sizeof img) &&
         load_thumbnail_file(img, tb) && tb->data))
      return 1;
  }
  /* Legacy .res lookup: only when "gamelist + files on disk" is on, or when
   * the system has no gamelist at all (behaviour unchanged for them). */
  if (settings_gamelist_dir || !have_root || !gl_has_list(root))
    return load_game_artwork(full_path, kind, tb) && tb->data;
  return 0;
}
