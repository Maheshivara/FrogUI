#include "gamelist.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAMELIST_MAX_BYTES (64L * 1024L * 1024L)
#define GAMELIST_MAX_GAMES 100000u

const MetaDataDecl gameDecls[] = {
    {"name", MD_STRING, "", false, "Name"},
    {"desc", MD_MULTILINE_STRING, "", false, "Description"},
    {"image", MD_IMAGE_PATH, "", false, "Image"},
    {"thumbnail", MD_IMAGE_PATH, "", false, "Thumb"},
    {"rating", MD_RATING, "0.000000", false, "Rating"},
    {"releasedate", MD_DATE, "not-a-date-time", false, "Released at"},
    {"developer", MD_STRING, "unknown", false, "Develop"},
    {"publisher", MD_STRING, "unknown", false, "Publisher"},
    {"genre", MD_STRING, "unknown", false, "Genre"},
    {"players", MD_INT, "1", false, "Players"},
    {"playcount", MD_INT, "0", true, "Play Count"},
    {"lastplayed", MD_TIME, "0", true, "Last Played"}};
const size_t gameDeclCount = sizeof(gameDecls) / sizeof(gameDecls[0]);

const MetaDataDecl folderDecls[] = {
    {"name", MD_STRING, "", false, "Name"},
    {"desc", MD_MULTILINE_STRING, "", false, "Description"},
    {"image", MD_IMAGE_PATH, "", false, "Image"},
    {"thumbnail", MD_IMAGE_PATH, "", false, "Thumb"}};
const size_t folderDeclCount = sizeof(folderDecls) / sizeof(folderDecls[0]);

static void set_error(char **out, const char *message) {
  if (out) {
    free(*out);
    size_t n = strlen(message);
    *out = (char *)malloc(n + 1);
    if (*out)
      memcpy(*out, message, n + 1);
  }
}

/* memmem for [s, end). */
static char *find_in(char *s, const char *end, const char *needle, size_t nl) {
  const char *last;
  if (end < s || (size_t)(end - s) < nl)
    return NULL;
  last = end - nl;
  while (s <= last) {
    s = (char *)memchr(s, needle[0], (size_t)(last - s) + 1);
    if (!s)
      return NULL;
    if (memcmp(s, needle, nl) == 0)
      return s;
    s++;
  }
  return NULL;
}

static size_t put_utf8(char *o, unsigned long cp) {
  if (cp < 0x80) {
    o[0] = (char)cp;
    return 1;
  }
  if (cp < 0x800) {
    o[0] = (char)(0xC0 | (cp >> 6));
    o[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    o[0] = (char)(0xE0 | (cp >> 12));
    o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    o[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  o[0] = (char)(0xF0 | (cp >> 18));
  o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  o[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

/* Decode entities IN PLACE over [s, e) and NUL-terminate. Output is never
 * longer than input (every entity is at least as long as its UTF-8 result), so
 * the write cursor never passes the read cursor. Returns the end (the NUL).
 * Unknown/invalid entities are kept literally. *e is overwritten with NUL. */
static char *decode_inplace(char *s, char *e) {
  char *r = (char *)memchr(s, '&', (size_t)(e - s));
  char *w;
  if (!r) { /* common case: nothing to decode */
    *e = '\0';
    return e;
  }
  w = r;
  while (r < e) {
    size_t rem = (size_t)(e - r);
    if (*r != '&') {
      *w++ = *r++;
    } else if (rem >= 5 && !memcmp(r, "&amp;", 5)) {
      *w++ = '&';
      r += 5;
    } else if (rem >= 4 && !memcmp(r, "&lt;", 4)) {
      *w++ = '<';
      r += 4;
    } else if (rem >= 4 && !memcmp(r, "&gt;", 4)) {
      *w++ = '>';
      r += 4;
    } else if (rem >= 6 && !memcmp(r, "&quot;", 6)) {
      *w++ = '"';
      r += 6;
    } else if (rem >= 6 && !memcmp(r, "&apos;", 6)) {
      *w++ = '\'';
      r += 6;
    } else if (rem >= 4 && r[1] == '#') {
      char *k = r + 2;
      unsigned long cp = 0;
      int hex = (*k == 'x' || *k == 'X');
      size_t digits = 0;
      if (hex)
        k++;
      while (k < e && digits < 8) {
        char c = *k;
        int d = (c >= '0' && c <= '9')          ? c - '0'
                : (hex && c >= 'a' && c <= 'f') ? c - 'a' + 10
                : (hex && c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                : -1;
        if (d < 0)
          break;
        cp = cp * (hex ? 16UL : 10UL) + (unsigned long)d;
        k++;
        digits++;
      }
      if (k < e && *k == ';' && digits && cp && cp <= 0x10FFFFUL &&
          !(cp >= 0xD800UL && cp <= 0xDFFFUL)) {
        w += put_utf8(w, cp);
        r = k + 1;
      } else {
        *w++ = *r++;
      }
    } else {
      *w++ = *r++;
    }
  }
  *w = '\0';
  return w;
}

/* Finds the next <game> or <game attr...> opening tag in [p, end). Skips
 * <gameList>, <games> and self-closed <game/>. */
static char *next_game(char *p, char *end, char **content) {
  while ((p = find_in(p, end, "<game", 5)) != NULL) {
    char c;
    if (end - p < 6)
      return NULL;
    c = p[5];
    if (c == '>') {
      *content = p + 6;
      return p;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      char *gt = (char *)memchr(p + 5, '>', (size_t)(end - (p + 5)));
      if (!gt)
        return NULL;
      if (gt[-1] != '/') {
        *content = gt + 1;
        return p;
      }
      p = gt + 1;
      continue;
    }
    p += 5;
  }
  return NULL;
}

/* One pass over a <game> body. Every element is validated locally, so a broken
 * element only loses itself, never its neighbours. Values are decoded in place
 * and vals[i] / *path point into the buffer. First occurrence of a tag wins. */
static void parse_block(char *b, char *e, const MetaDataDecl *decls, size_t dc,
                        const char **vals, bool *seen, const char **path) {
  char *p = b;
  while (p < e) {
    char *name, *q, *gt, *vs, *ve, *after, *vend;
    size_t nl, i;
    bool cdata;

    p = (char *)memchr(p, '<', (size_t)(e - p));
    if (!p)
      break;
    name = q = p + 1;
    while (q < e && (isalnum((unsigned char)*q) || *q == '_'))
      q++;
    if (q == name || q >= e) { /* </x>, <!-- -->, junk */
      p++;
      continue;
    }
    nl = (size_t)(q - name);
    gt = (char *)memchr(q, '>', (size_t)(e - q));
    if (!gt)
      break;
    if (gt[-1] == '/') { /* <tag/> : empty */
      p = gt + 1;
      continue;
    }
    vs = gt + 1;
    cdata = (e - vs >= 9 && memcmp(vs, "<![CDATA[", 9) == 0);
    if (cdata) {
      vs += 9;
      ve = find_in(vs, e, "]]>", 3);
      if (!ve)
        break;
      after = ve + 3;
      /* tolerate whitespace before the closing tag */
      while (after < e && isspace((unsigned char)*after))
        after++;
    } else {
      ve = (char *)memchr(vs, '<', (size_t)(e - vs));
      if (!ve)
        break;
      after = ve;
    }
    /* must be exactly </name> */
    if (!((size_t)(e - after) >= nl + 3 && after[0] == '<' && after[1] == '/' &&
          memcmp(after + 2, name, nl) == 0 && after[2 + nl] == '>')) {
      p = after > p ? after : p + 1; /* malformed: resume, always progress */
      continue;
    }
    p = after + nl + 3;

    {
      int idx = -1;
      bool is_path = (nl == 4 && memcmp(name, "path", 4) == 0);
      if (!is_path) {
        for (i = 0; i < dc; i++)
          if (strncmp(decls[i].key, name, nl) == 0 && decls[i].key[nl] == 0) {
            idx = (int)i;
            break;
          }
        if (idx < 0 || seen[idx])
          continue;
      } else if (*path) {
        continue;
      }

      if (cdata) {
        vend = ve;
        *vend = '\0';
      } else {
        vend = decode_inplace(vs, ve);
      }
      while (vs < vend && isspace((unsigned char)*vs))
        vs++;
      while (vend > vs && isspace((unsigned char)vend[-1]))
        *--vend = '\0';

      if (is_path)
        *path = vs;
      else {
        vals[idx] = vs;
        seen[idx] = true;
      }
    }
  }
}

void gamelist_free(GameList *list) {
  if (!list)
    return;
  free(list->games);
  free(list->slab);
  free(list->buf);
  memset(list, 0, sizeof *list);
}

bool gamelist_load(const char *filename, const MetaDataDecl *decls,
                   size_t decl_count, GameList *out, char **error_message) {
  FILE *fp;
  char *buf, *end, *cur, *cur_content;
  long file_size;
  size_t len, n = 0, cnt = 0, i;
  Game *games;
  const char **slab;
  bool *seen;

  if (error_message)
    *error_message = NULL;
  if (!filename || !decls || !out || decl_count > 1024) {
    set_error(error_message, "Invalid argument.");
    return false;
  }
  memset(out, 0, sizeof *out);
  out->metadata_count = decl_count;
  out->decls = decls;

  fp = fopen(filename, "rb");
  if (!fp) {
    set_error(error_message, "Could not open gamelist file.");
    return false;
  }
  if (fseek(fp, 0, SEEK_END) != 0 || (file_size = ftell(fp)) < 0 ||
      fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    set_error(error_message, "Could not determine file size.");
    return false;
  }
  if (file_size > GAMELIST_MAX_BYTES) {
    fclose(fp);
    set_error(error_message, "Gamelist file is too large.");
    return false;
  }
  buf = (char *)malloc((size_t)file_size + 1);
  if (!buf) {
    fclose(fp);
    set_error(error_message, "Out of memory reading gamelist.");
    return false;
  }
  len = fread(buf, 1, (size_t)file_size, fp);
  if (ferror(fp)) {
    free(buf);
    fclose(fp);
    set_error(error_message, "Error reading gamelist file.");
    return false;
  }
  fclose(fp);
  buf[len] = '\0';
  end = buf + len;

  /* Pass 1: count openings so games[] and the values slab are allocated
   * exactly once (no realloc growth, no per-game malloc). */
  for (cur = next_game(buf, end, &cur_content); cur && n < GAMELIST_MAX_GAMES;
       cur = next_game(cur_content, end, &cur_content))
    n++;

  games = (Game *)calloc(n ? n : 1, sizeof(Game));
  slab = (const char **)malloc((n ? n : 1) * (decl_count ? decl_count : 1) *
                               sizeof(char *));
  seen = (bool *)malloc(decl_count ? decl_count : 1);
  if (!games || !slab || !seen) {
    free(games);
    free(slab);
    free(seen);
    free(buf);
    set_error(error_message, "Out of memory parsing game metadata.");
    return false;
  }

  /* Pass 2: parse. A block ends at its </game>, or at the next <game> opening
   * (unclosed entry), or at EOF (truncated file). */
  cur = next_game(buf, end, &cur_content);
  while (cur && cnt < n) {
    char *nc = NULL;
    char *nxt = next_game(cur_content, end, &nc);
    char *limit = nxt ? nxt : end;
    char *close = find_in(cur_content, limit, "</game>", 7);
    char *block_end = close ? close : limit;
    const char **vals = slab + cnt * (decl_count ? decl_count : 1);
    const char *path = NULL;

    for (i = 0; i < decl_count; i++) {
      vals[i] = decls[i].default_value;
      seen[i] = false;
    }
    parse_block(cur_content, block_end, decls, decl_count, vals, seen, &path);
    if (path && *path) {
      games[cnt].path = path;
      games[cnt].values = vals;
      cnt++;
    }
    cur = nxt;
    cur_content = nc;
  }
  free(seen);

  out->games = games;
  out->count = cnt;
  out->slab = slab;
  out->buf = buf;
  out->buf_len = len;
  return true;
}

bool gamelist_load_games(const char *filename, GameList *out,
                         char **error_message) {
  return gamelist_load(filename, gameDecls, gameDeclCount, out, error_message);
}

static bool in_buf(const GameList *l, const char *p) {
  return p >= l->buf && p < l->buf + l->buf_len + 1;
}

bool gamelist_compact(GameList *l, unsigned keep_mask) {
  size_t i, j, total = 0;
  char *arena, *w;
  if (!l || !l->buf || !l->count)
    return true;

  for (i = 0; i < l->count; i++) {
    total += strlen(l->games[i].path) + 1;
    for (j = 0; j < l->metadata_count && j < 32; j++) {
      const char *v = l->games[i].values[j];
      if (((keep_mask >> j) & 1u) && in_buf(l, v))
        total += strlen(v) + 1;
    }
  }
  arena = (char *)malloc(total);
  if (!arena)
    return false; /* list untouched */

  w = arena;
  for (i = 0; i < l->count; i++) {
    Game *g = &l->games[i];
    size_t n = strlen(g->path) + 1;
    memcpy(w, g->path, n);
    g->path = w;
    w += n;
    for (j = 0; j < l->metadata_count; j++) {
      const char *v = g->values[j];
      if (!in_buf(l, v))
        continue; /* already a static default */
      if (j < 32 && ((keep_mask >> j) & 1u)) {
        n = strlen(v) + 1;
        memcpy(w, v, n);
        g->values[j] = w;
        w += n;
      } else {
        g->values[j] = l->decls[j].default_value;
      }
    }
  }
  free(l->buf);
  l->buf = arena;
  l->buf_len = total;
  return true;
}

const char *game_get(const Game *game, const MetaDataDecl *decls,
                     size_t decl_count, const char *key) {
  size_t i;
  if (!game || !game->values || !decls || !key)
    return NULL;
  for (i = 0; i < decl_count; ++i)
    if (strcmp(decls[i].key, key) == 0)
      return game->values[i];
  return NULL;
}
