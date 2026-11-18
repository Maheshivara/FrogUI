#include "gamelist.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static char *str_dup_n(const char *s, size_t n) {
  char *p = (char *)malloc(n + 1);
  if (!p)
    return NULL;
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

static char *str_dup(const char *s) {
  if (!s)
    s = "";
  return str_dup_n(s, strlen(s));
}

static void set_error(char **out, const char *message) {
  if (out) {
    free(*out);
    *out = str_dup(message);
  }
}

/* Decode XML's five predefined entities. Unknown entities are preserved. */
static char *xml_decode(const char *src, size_t len) {
  char *out = (char *)malloc(len + 1);
  size_t i = 0, j = 0;

  if (!out)
    return NULL;

  while (i < len) {
    if (src[i] == '&') {
      if (i + 5 <= len && memcmp(src + i, "&amp;", 5) == 0) {
        out[j++] = '&';
        i += 5;
        continue;
      } else if (i + 4 <= len && memcmp(src + i, "&lt;", 4) == 0) {
        out[j++] = '<';
        i += 4;
        continue;
      } else if (i + 4 <= len && memcmp(src + i, "&gt;", 4) == 0) {
        out[j++] = '>';
        i += 4;
        continue;
      } else if (i + 6 <= len && memcmp(src + i, "&quot;", 6) == 0) {
        out[j++] = '"';
        i += 6;
        continue;
      } else if (i + 6 <= len && memcmp(src + i, "&apos;", 6) == 0) {
        out[j++] = '\'';
        i += 6;
        continue;
      } else if (i + 3 <= len && src[i + 1] == '#') {
        /* &#10; / &#xA; - scrapers emit these for newlines and quotes. */
        size_t k = i + 2;
        unsigned long cp = 0;
        int hex = (k < len && (src[k] == 'x' || src[k] == 'X'));
        size_t digits = 0;
        if (hex)
          k++;
        while (k < len && src[k] != ';' && digits < 6) {
          char c = src[k];
          int d = (c >= '0' && c <= '9')   ? c - '0'
                  : (hex && c >= 'a' && c <= 'f') ? c - 'a' + 10
                  : (hex && c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                  : -1;
          if (d < 0)
            break;
          cp = cp * (hex ? 16 : 10) + (unsigned long)d;
          k++;
          digits++;
        }
        if (k < len && src[k] == ';' && digits > 0 && cp > 0 && cp <= 0xFFFF) {
          /* Output never exceeds the entity length (>=4 chars -> <=3 bytes). */
          if (cp < 0x80)
            out[j++] = (char)cp;
          else if (cp < 0x800) {
            out[j++] = (char)(0xC0 | (cp >> 6));
            out[j++] = (char)(0x80 | (cp & 0x3F));
          } else {
            out[j++] = (char)(0xE0 | (cp >> 12));
            out[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[j++] = (char)(0x80 | (cp & 0x3F));
          }
          i = k + 1;
          continue;
        }
      }
    }
    out[j++] = src[i++];
  }

  out[j] = '\0';
  return out;
}

/* memmem restricted to [s, end). The old code used strstr(), which scans to
 * the end of the WHOLE file whenever a tag is absent from a game: with ~3000
 * games and a few missing fields that is O(n^2) over a multi-MB buffer. */
static const char *find_in(const char *s, const char *end, const char *needle,
                           size_t nl) {
  const char *last;
  if (end < s || (size_t)(end - s) < nl)
    return NULL;
  last = end - nl;
  while (s <= last) {
    s = (const char *)memchr(s, needle[0], (size_t)(last - s) + 1);
    if (!s)
      return NULL;
    if (memcmp(s, needle, nl) == 0)
      return s;
    s++;
  }
  return NULL;
}

/* Reads a plain <tag>value</tag> element within [start, end). */
static char *read_element(const char *start, const char *end, const char *tag) {
  char open_tag[48], close_tag[50];
  size_t n = strlen(tag), ol, cl;
  const char *value_start, *value_end;

  if (n > 40)
    return NULL;
  ol = (size_t)snprintf(open_tag, sizeof open_tag, "<%s>", tag);
  cl = (size_t)snprintf(close_tag, sizeof close_tag, "</%s>", tag);

  value_start = find_in(start, end, open_tag, ol);
  if (!value_start)
    return NULL;
  value_start += ol;
  value_end = find_in(value_start, end, close_tag, cl);
  if (!value_end)
    return NULL;
  return xml_decode(value_start, (size_t)(value_end - value_start));
}

static void free_one_game(Game *game, size_t metadata_count) {
  size_t i;
  if (!game)
    return;

  free(game->path);
  for (i = 0; game->values && i < metadata_count; ++i)
    free(game->values[i]);
  free(game->values);

  game->path = NULL;
  game->values = NULL;
}

void gamelist_free(GameList *list) {
  size_t i;
  if (!list)
    return;

  for (i = 0; i < list->count; ++i)
    free_one_game(&list->games[i], list->metadata_count);

  free(list->games);
  list->games = NULL;
  list->count = 0;
  list->metadata_count = 0;
}

static bool append_game(GameList *list, size_t *capacity, const char *begin,
                        const char *end, const MetaDataDecl *decls,
                        size_t decl_count) {
  Game game;
  Game *new_games;
  size_t i;

  memset(&game, 0, sizeof(game));
  game.path = read_element(begin, end, "path");
  if (!game.path || !game.path[0]) { /* nothing to match against: skip */
    free(game.path);
    return true;
  }
  game.values = (char **)calloc(decl_count ? decl_count : 1, sizeof(char *));
  if (!game.values) {
    free(game.path);
    return false;
  }

  for (i = 0; i < decl_count; ++i) {
    game.values[i] = read_element(begin, end, decls[i].key);
    if (!game.values[i])
      game.values[i] = str_dup(decls[i].default_value);

    if (!game.values[i]) {
      free_one_game(&game, decl_count);
      return false;
    }
  }

  if (list->count >= *capacity) {
    size_t nc = *capacity ? *capacity * 2 : 256;
    new_games = (Game *)realloc(list->games, nc * sizeof(Game));
    if (!new_games) {
      free_one_game(&game, decl_count);
      return false;
    }
    list->games = new_games;
    *capacity = nc;
  }

  list->games[list->count++] = game;
  return true;
}

bool gamelist_load(const char *filename, const MetaDataDecl *decls,
                   size_t decl_count, GameList *out, char **error_message) {
  FILE *fp;
  char *xml;
  long file_size;
  size_t bytes_read;
  const char *cursor;
  size_t capacity = 0;

  if (error_message)
    *error_message = NULL;
  if (!filename || !decls || !out) {
    set_error(error_message, "Invalid argument.");
    return false;
  }

  out->games = NULL;
  out->count = 0;
  out->metadata_count = decl_count;

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

  xml = (char *)malloc((size_t)file_size + 1);
  if (!xml) {
    fclose(fp);
    set_error(error_message, "Out of memory reading gamelist.");
    return false;
  }

  bytes_read = fread(xml, 1, (size_t)file_size, fp);
  if (ferror(fp)) {
    free(xml);
    fclose(fp);
    set_error(error_message, "Error reading gamelist file.");
    return false;
  }
  fclose(fp);
  xml[bytes_read] = '\0';

  cursor = xml;
  while ((cursor = strstr(cursor, "<game")) != NULL) {
    /* Accept <game> AND <game id=".." source=".."> (ES/Skraper output), but
     * not <gameList>. */
    const char *begin;
    const char *end;
    char c5 = cursor[5];
    if (c5 == '>')
      begin = cursor + 6;
    else if (c5 == ' ' || c5 == '\t' || c5 == '\n' || c5 == '\r') {
      const char *gt = strchr(cursor + 5, '>');
      if (!gt) {
        cursor += 5;
        continue;
      }
      begin = gt + 1;
    } else {
      cursor += 5;
      continue;
    }
    end = strstr(begin, "</game>");

    if (!end) {
      free(xml);
      gamelist_free(out);
      set_error(error_message, "Found a <game> without a closing </game>.");
      return false;
    }

    if (!append_game(out, &capacity, begin, end, decls, decl_count)) {
      free(xml);
      gamelist_free(out);
      set_error(error_message, "Out of memory parsing game metadata.");
      return false;
    }

    cursor = end + 7;
  }

  free(xml);
  return true;
}

bool gamelist_load_games(const char *filename, GameList *out,
                         char **error_message) {
  return gamelist_load(filename, gameDecls, gameDeclCount, out, error_message);
}

const char *game_get(const Game *game, const MetaDataDecl *decls,
                     size_t decl_count, const char *key) {
  size_t i;
  if (!game || !game->values || !decls || !key)
    return NULL;

  for (i = 0; i < decl_count; ++i) {
    if (strcmp(decls[i].key, key) == 0)
      return game->values[i];
  }
  return NULL;
}
