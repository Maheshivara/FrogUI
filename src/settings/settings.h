#ifndef SETTINGS_H
#define SETTINGS_H

#include "../app/app_common.h"

/* --- Settings ---
 * Stored at /mnt/sdcard/frogui/settings.txt, key=value format.
 * Two options: theme + font. */
#define SETTINGS_DIR "/mnt/sdcard/frogui"
#define SETTINGS_FILE SETTINGS_DIR "/settings.txt"
enum { STYLE_VERTICAL, STYLE_HORIZONTAL, STYLE_SYSTEM, STYLE_COUNT };
enum { ROM_SOURCE_SD, ROM_SOURCE_OTG, ROM_SOURCE_COUNT };
#define FILTER_COUNT 2
#define SETTINGS_BRIGHTNESS_STEP 5
/* Apply only the setting being previewed. The old generic settings_apply()
 * reloaded the current font and fsync'd sndgain.txt after every Left/Right
 * press, even for unrelated toggles. Persistent brightness/volume writes are
 * deferred until menu exit; their on-screen preview remains immediate. */
/* Live-volume debounce: the slider applies the level audibly on EVERY step
 * (hardware mirror), but commits the durable persistentmem write at most once
 * per VOLUME_COMMIT_MS while the user is still sliding, plus a final commit
 * shortly after the last step - so the physical volume meter follows the
 * slider in real time, without one EEPROM write per repeat step.  A step that
 * arrives later than the window commits immediately (slow adjustment = live
 * commit too).  The menu-exit path (settings_save_file) still writes the
 * final value unconditionally, covering any remaining edge. */
#define VOLUME_COMMIT_MS 500
extern int settings_wallpaper_idx;
extern int settings_wallpaper_fit;
extern int settings_theme_pack_idx;
extern int settings_icon_pack_idx;
extern bool settings_menu_active;
extern int settings_menu_idx;
extern int settings_theme_idx;
extern int settings_font_idx;
extern int settings_font_size;
extern int settings_brightness;
extern int settings_bl_reassert;
extern int settings_audio_mute_reassert;
extern int settings_filter_idx;
extern int settings_filter_idx_on_enter;
extern int settings_quick_resume;
extern int settings_custom_aspect_ratios;
extern int settings_autosave_autoload;
extern int settings_anim;
extern int settings_menu_sounds;
extern int settings_style;
extern int settings_center_text;
extern int settings_friendly_names;
extern int settings_gamelist_dir;
extern int settings_list_info;
extern int settings_hide_empty;
extern int settings_hide_extensions;
extern int settings_backgrounds;
extern int settings_background_dim;
extern int settings_file_cache;
extern int settings_battery_color;
extern int settings_stock_battery;
extern int settings_language;
extern int settings_game_switcher;
extern int settings_load_recents;
extern int settings_rom_source;
extern const char *rom_source_names[ROM_SOURCE_COUNT];
extern int settings_disable_sleep;
extern int settings_volume;
extern const char *style_names[STYLE_COUNT];
extern long long volume_last_commit_ms;
extern int volume_committed_level;
const char *treefrogui_version(void);

void settings_apply(void);
void settings_volume_commit_live(void);
void settings_volume_commit_final(void);
void settings_load_file(void);
bool otg_roms_available(void);
void resolve_roms_root(void);
void settings_save_file(void);
/* Called by render.c (battery glyph): 1 = solid colour mode. Must stay global.
 */
int frogui_battery_color_mode(void);
#endif
