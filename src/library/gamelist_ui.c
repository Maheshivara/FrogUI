/* gamelist_ui.c - gamelist.xml support for the browser: listing, titles,
 * artwork and info. Depends on gl_browser (cache/lookup) and gamelist (parser). */
#include "app.h"
#include "stb_image.h"

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
  static const char *skip[] = {"media",  "images",          "videos",
                               "manuals", "downloaded_images", "downloaded_media",
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

#define GL_THUMB_MAX 320 /* longest side kept in RAM */

bool load_thumbnail_file(const char *path, Thumbnail *tb) {
  int w, h, ch;
  unsigned char *img = stbi_load(path, &w, &h, &ch, 4);
  if (!img)
    return false;
  int dw = w, dh = h;
  if (w > GL_THUMB_MAX || h > GL_THUMB_MAX) {
    if (w >= h) { dw = GL_THUMB_MAX; dh = h * GL_THUMB_MAX / w; }
    else        { dh = GL_THUMB_MAX; dw = w * GL_THUMB_MAX / h; }
    if (dh < 1) dh = 1;
    if (dw < 1) dw = 1;
  }
  uint16_t *data = (uint16_t *)malloc((size_t)dw * dh * sizeof(uint16_t));
  uint8_t *alpha = (uint8_t *)malloc((size_t)dw * dh);
  if (!data || !alpha) {
    free(data);
    free(alpha);
    stbi_image_free(img);
    return false;
  }
  for (int y = 0; y < dh; y++) {
    const unsigned char *row = img + (size_t)(y * h / dh) * w * 4;
    for (int x = 0; x < dw; x++) {
      const unsigned char *p = row + (size_t)(x * w / dw) * 4;
      data[(size_t)y * dw + x] =
          (uint16_t)(((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3));
      alpha[(size_t)y * dw + x] = p[3];
    }
  }
  stbi_image_free(img);
  memset(tb, 0, sizeof *tb);
  tb->data = data;
  tb->alpha = alpha;
  tb->width = dw;
  tb->height = dh;
  return true;
}

int game_art_load(const char *full_path, int kind, Thumbnail *tb) {
  char root[MAX_PATH_LEN], img[MAX_PATH_LEN];
  bool have_root = gamelist_root_for(full_path, root, sizeof root);
  const Game *g = have_root ? gl_lookup(root, full_path) : NULL;
  if (g) {
    /* ES convention: thumbnail = box art, image = screenshot/title. */
    const char *k1 = (kind == ARTWORK_BOXART) ? "thumbnail" : "image";
    const char *k2 = (kind == ARTWORK_BOXART) ? "image" : "thumbnail";
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
