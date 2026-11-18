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

typedef struct {
  char *path;    /* The <path> element is separate from metadata. */
  char **values; /* Indexed in the same order as the declarations. */
} Game;

typedef struct {
  Game *games;
  size_t count;
  size_t metadata_count; /* Stored so gamelist_free also works with custom
                            declarations. */
} GameList;

extern const MetaDataDecl gameDecls[];
extern const size_t gameDeclCount;
extern const MetaDataDecl folderDecls[];
extern const size_t folderDeclCount;

/* Missing fields are filled with the declaration's default_value. */
bool gamelist_load(const char *filename, const MetaDataDecl *decls,
                   size_t decl_count, GameList *out, char **error_message);

bool gamelist_load_games(const char *filename, GameList *out,
                         char **error_message);

const char *game_get(const Game *game, const MetaDataDecl *decls,
                     size_t decl_count, const char *key);

void gamelist_free(GameList *list);

#endif
