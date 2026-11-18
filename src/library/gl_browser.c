#include "gl_browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define GL_PATH 512

/* Slimmer than gameDecls: the browser never shows publisher/playcount/
 * lastplayed, and every declaration costs one malloc per game. */
static const MetaDataDecl decls[] = {
    {"name", MD_STRING, "", false, "Name"},
    {"desc", MD_MULTILINE_STRING, "", false, "Description"},
    {"image", MD_IMAGE_PATH, "", false, "Image"},
    {"thumbnail", MD_IMAGE_PATH, "", false, "Thumb"},
    {"rating", MD_RATING, "0.000000", false, "Rating"},
    {"releasedate", MD_DATE, "not-a-date-time", false, "Released at"},
    {"developer", MD_STRING, "unknown", false, "Develop"},
    {"genre", MD_STRING, "unknown", false, "Genre"},
    {"players", MD_INT, "1", false, "Players"}};
#define NDECLS (sizeof(decls) / sizeof(decls[0]))

typedef struct {
  const char *key; /* points into game->path, normalized */
  const Game *game;
} GlKey;

static struct {
  char root[GL_PATH];
  time_t mtime;
  off_t size;
  bool attempted; /* root/mtime/size describe what we tried to load */
  bool ok;        /* list holds >= 1 game */
  GameList list;
  GlKey *keys;
  size_t nkeys;
} gl;

/* "<root>/a/b.gb" or "./a/b.gb" -> "a/b.gb" */
static const char *rel_key(const char *root, const char *p) {
  size_t rl = strlen(root);
  if (rl && strncmp(p, root, rl) == 0 && p[rl] == '/')
    return p + rl + 1;
  while (p[0] == '.' && p[1] == '/')
    p += 2;
  return p;
}

static int key_cmp(const void *a, const void *b) {
  return strcasecmp(((const GlKey *)a)->key, ((const GlKey *)b)->key);
}

static void gl_free(void) {
  free(gl.keys);
  gl.keys = NULL;
  gl.nkeys = 0;
  gamelist_free(&gl.list);
  gl.ok = false;
}

static void load_root(const char *root) {
  char file[GL_PATH];
  struct stat st;
  char *err = NULL;

  gl_free();
  strncpy(gl.root, root, sizeof gl.root - 1);
  gl.root[sizeof gl.root - 1] = '\0';
  gl.attempted = true;
  gl.mtime = 0;
  gl.size = 0;

  snprintf(file, sizeof file, "%s/gamelist.xml", root);
  if (stat(file, &st) != 0)
    return;
  gl.mtime = st.st_mtime;
  gl.size = st.st_size;

  if (!gamelist_load(file, decls, NDECLS, &gl.list, &err)) {
    free(err);
    gamelist_free(&gl.list);
    return;
  }
  free(err);
  if (gl.list.count == 0)
    return;

  gl.keys = (GlKey *)malloc(gl.list.count * sizeof(GlKey));
  if (!gl.keys) {
    gamelist_free(&gl.list);
    return;
  }
  for (size_t i = 0; i < gl.list.count; i++) {
    gl.keys[i].key = rel_key(root, gl.list.games[i].path);
    gl.keys[i].game = &gl.list.games[i];
  }
  gl.nkeys = gl.list.count;
  qsort(gl.keys, gl.nkeys, sizeof(GlKey), key_cmp);
  gl.ok = true;
}

bool gl_open(const char *sys_root) {
  char file[GL_PATH];
  struct stat st;

  if (!sys_root || !sys_root[0])
    return false;
  if (gl.attempted && strcmp(gl.root, sys_root) == 0) {
    snprintf(file, sizeof file, "%s/gamelist.xml", sys_root);
    if (stat(file, &st) == 0) {
      if (st.st_mtime == gl.mtime && st.st_size == gl.size)
        return gl.ok;
    } else if (gl.mtime == 0) {
      return false; /* still no file */
    }
  }
  load_root(sys_root);
  return gl.ok;
}

const Game *gl_lookup(const char *sys_root, const char *full_path) {
  if (!sys_root || !sys_root[0] || !full_path)
    return NULL;
  if (!gl.attempted || strcmp(gl.root, sys_root) != 0)
    load_root(sys_root);
  if (!gl.ok)
    return NULL;
  GlKey probe = {rel_key(sys_root, full_path), NULL};
  GlKey *hit =
      (GlKey *)bsearch(&probe, gl.keys, gl.nkeys, sizeof(GlKey), key_cmp);
  return hit ? hit->game : NULL;
}

const char *gl_get(const Game *g, const char *key) {
  if (!g || !g->values || !key)
    return "";
  for (size_t i = 0; i < NDECLS; i++) {
    if (strcmp(decls[i].key, key) != 0)
      continue;
    const char *v = g->values[i];
    if (!v || strcmp(v, decls[i].default_value) == 0)
      return "";
    return v;
  }
  return "";
}

bool gl_image_path(const char *sys_root, const Game *g, const char *key,
                   char *out, size_t n) {
  const char *v = gl_get(g, key);
  if (!v[0] || !out || n == 0)
    return false;
  if (v[0] == '/') {
    snprintf(out, n, "%s", v);
  } else {
    while (v[0] == '.' && v[1] == '/')
      v += 2;
    snprintf(out, n, "%s/%s", sys_root, v);
  }
  return true;
}

void gl_list_dir(const char *sys_root, const char *dir_path, gl_entry_cb cb,
                 void *ud) {
  const char *rd;
  size_t rl, dl;
  char last_dir[256] = "";

  if (!cb || !sys_root || !dir_path)
    return;
  if (!gl.attempted || strcmp(gl.root, sys_root) != 0)
    load_root(sys_root);
  if (!gl.ok)
    return;

  /* dir_path relative to the system folder ("" = the system root). */
  rl = strlen(sys_root);
  rd = dir_path;
  if (strncmp(dir_path, sys_root, rl) == 0)
    rd = dir_path + rl;
  while (*rd == '/')
    rd++;
  dl = strlen(rd);

  /* keys are sorted case-insensitively, so every key under "a/" is
   * contiguous and a single "last emitted dir" is enough to deduplicate. */
  for (size_t i = 0; i < gl.nkeys; i++) {
    const char *k = gl.keys[i].key;
    if (dl) {
      if (strncasecmp(k, rd, dl) != 0 || k[dl] != '/')
        continue;
      k += dl + 1;
    }
    const char *slash = strchr(k, '/');
    if (slash) {
      size_t n = (size_t)(slash - k);
      char name[256];
      if (n == 0 || n >= sizeof name)
        continue;
      memcpy(name, k, n);
      name[n] = '\0';
      if (strcasecmp(name, last_dir) == 0)
        continue;
      strcpy(last_dir, name);
      cb(name, 1, ud);
    } else if (*k) {
      cb(k, 0, ud);
    }
  }
}

bool gl_has_list(const char *sys_root) {
  if (!sys_root || !sys_root[0])
    return false;
  if (!gl.attempted || strcmp(gl.root, sys_root) != 0)
    load_root(sys_root);
  return gl.ok;
}

void gl_close(void) {
  gl_free();
  gl.attempted = false;
  gl.root[0] = '\0';
}
