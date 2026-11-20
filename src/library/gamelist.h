#ifndef GAMELIST_H
#define GAMELIST_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  MD_STRING,
  MD_MULTILINE_STRING,
  MD_IMAGE_PATH,
  MD_RATING,
  MD_DATE,
  MD_TIME,
  MD_INT
} MetaDataType;

typedef struct {
  const char *key;
  MetaDataType type;
  const char *default_value;
  bool statistic;
  const char *display_name;
} MetaDataDecl;

/* Strings are NOT individually allocated. They point either into the list's
 * text buffer or at a decl's static default_value. Treat them as read-only and
 * valid until gamelist_free(). */
typedef struct {
  const char *path;
  const char **values; /* indexed like the declarations */
} Game;

typedef struct {
  Game *games;
  size_t count;
  size_t metadata_count;
  const MetaDataDecl *decls;
  char *buf;
  size_t buf_len;
  const char **slab;
} GameList;

extern const MetaDataDecl gameDecls[];
extern const size_t gameDeclCount;
extern const MetaDataDecl folderDecls[];
extern const size_t folderDeclCount;

/* Tolerant loader: damaged/truncated <game> entries are skipped, the rest of
 * the file is still returned. Fails only on I/O error, OOM or absurd size.
 * Missing fields point at the declaration's default_value. */
bool gamelist_load(const char *filename, const MetaDataDecl *decls,
                   size_t decl_count, GameList *out, char **error_message);

bool gamelist_load_games(const char *filename, GameList *out,
                         char **error_message);

/* Copy only the fields whose bit is set in keep_mask (bit i = decl
 * i) into a tight arena and release the raw XML buffer. Unkept fields read as
 * their default. Use after load when e.g. "desc" is not needed up front. */
bool gamelist_compact(GameList *list, unsigned keep_mask);

const char *game_get(const Game *game, const MetaDataDecl *decls,
                     size_t decl_count, const char *key);

void gamelist_free(GameList *list);

#endif
