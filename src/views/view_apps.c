#include "app.h"
int app_def_count(void);

const AppEntry app_defs[] = {
    {"activity", "app.activity", "Activity Tracker", NULL, NULL, NULL},
    {"frogshell", "app.frogshell", "FrogShell", NULL, NULL, FROGSHELL_CORE},
    {"usbmode", "app.usb_mode", "USB mode", NULL, NULL, USB_MODE_BIN},
    {"ebook", "app.ebook_reader", "Ebook Reader", "Ebook", "ebooks", NULL},
    {"images", "app.image_viewer", "Image Viewer", "images", "photos", NULL},
    {"videos", "app.videos", "Videos", "videos", "video", NULL},
    {"music", "app.music_mp3", "Music / MP3", "music", "audio", NULL},
    {"rockbox", "app.rockbox", "Rockbox", "rockbox", NULL, NULL},
    {"shutdown", "app.shutdown", "Shutdown", NULL, NULL, SHUTDOWN_BIN},
};

int app_def_count(void) {
  return (int)(sizeof(app_defs) / sizeof(app_defs[0]));
}
const char *app_folder_path(int index, char *out, size_t out_size) {
  if (index < 0 || index >= app_def_count())
    return NULL;
  const AppEntry *a = &app_defs[index];
  if (!a->folder_a)
    return NULL;
  const char *candidates[3] = {a->folder_a, a->folder_b, NULL};
  for (int i = 0; candidates[i]; i++) {
    snprintf(out, out_size, "%s/%s", ROMS_PATH, candidates[i]);
    struct stat st;
    if (stat(out, &st) == 0 && S_ISDIR(st.st_mode))
      return out;
  }
  return NULL;
}
int is_app_folder_name(const char *name) {
  return name && (!strcasecmp(name, "rockbox") || !strcasecmp(name, "music") ||
                  !strcasecmp(name, "audio") || !strcasecmp(name, "videos") ||
                  !strcasecmp(name, "video") || !strcasecmp(name, "images") ||
                  !strcasecmp(name, "photos") || !strcasecmp(name, "ebook") ||
                  !strcasecmp(name, "ebooks"));
}
void scan_apps_tab(void) {
  strncpy(current_path, ROMS_PATH, MAX_PATH_LEN - 1);
  current_path[MAX_PATH_LEN - 1] = '\0';
  apps_root_path[0] = '\0';
  entry_count = 0;
  const char *tf_device = getenv("TF_DEVICE");
  for (int i = 0; i < app_def_count(); i++) {
    char path[MAX_PATH_LEN];
    /* SF3000 has a physical power toggle; expose software shutdown on
     * devices whose power control is momentary (SF3100/SF3500/R36*). */
    if (!strcmp(app_defs[i].key, "shutdown") && tf_device &&
        !strcasecmp(tf_device, "SF3000"))
      continue;
    if (!app_defs[i].bin && !strcmp(app_defs[i].key, "activity")) {
      if (access("/mnt/sdcard/frogui/playtime.txt", R_OK) != 0)
        continue;
    } else if (app_defs[i].bin) {
      if (access(app_defs[i].bin, X_OK) != 0)
        continue;
    } else if (!app_folder_path(i, path, sizeof path))
      continue;
    if (entry_count >= entry_capacity) {
      entry_capacity =
          entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
      entries = realloc(entries, (size_t)entry_capacity * sizeof(*entries));
      if (!entries) {
        entry_count = 0;
        return;
      }
    }
    strncpy(entries[entry_count].name, app_defs[i].key,
            sizeof(entries[entry_count].name) - 1);
    entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';
    entries[entry_count].is_dir = 1;
    entry_count++;
  }
  viewing_apps = true;
  viewing_activity = false;
  apps_browsing = false;
  viewing_recents = false;
  viewing_favourites = false;
  viewing_search = false;
  selected_index = apps_tab_selected;
  if (selected_index >= entry_count)
    selected_index = entry_count > 0 ? entry_count - 1 : 0;
  if (selected_index < 0)
    selected_index = 0;
  scroll_offset = 0;
}
const char *app_label(const char *key) {
  for (size_t i = 0; i < (size_t)app_def_count(); i++)
    if (strcasecmp(key, app_defs[i].key) == 0)
      return tr_or(app_defs[i].label_key, app_defs[i].label_en);
  return key;
}
