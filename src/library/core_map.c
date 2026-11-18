/* core_map.c - which libretro core / standalone binary runs a ROM.
 * Pure data + lookups: no UI state, no I/O. */
#include <string.h>
#include <strings.h>
#include "core_map.h"

/* Console → core mapping (folder name → libretro .so)
 * Folder names match /mnt/sdcard/roms/ subdirectories (gb300_multicore
 * convention). */
const ConsoleMapping console_mappings[] = {
    /* NES */
    {"nes", CORES_PATH "/fceumm_libretro.so"},
    {"nesq", CORES_PATH "/quicknes_libretro.so"},
    {"nest", CORES_PATH "/nestopia_libretro.so"},
    {"FC", CORES_PATH "/fceumm_libretro.so"},
    {"fds", CORES_PATH
     "/fceumm_libretro.so"}, /* Famicom Disk System (needs disksys.rom BIOS) */
    {"NES", CORES_PATH "/quicknes_libretro.so"},
    /* SNES */
    {"snes", CORES_PATH "/snes9x2005_plus_libretro.so"},
    {"snes02", CORES_PATH "/snes9x2002_libretro.so"},
    {"SFC", CORES_PATH "/snes9x2005_plus_libretro.so"},
    /* Game Boy */
    {"gb", CORES_PATH "/gambatte_libretro.so"},
    {"gbc", CORES_PATH "/gambatte_libretro.so"},
    {"gbgb", CORES_PATH "/gearboy_libretro.so"},
    {"gbb", CORES_PATH "/tgbdual_libretro.so"},
    {"dblcherrygb", CORES_PATH "/gambatte_libretro.so"},
    /* GBA */
    {"gba", CORES_PATH "/gpsp_libretro.so"}, /* upstream libretro/gpsp */
    {"GBA", CORES_PATH "/gpsp_libretro.so"},
    {"gbac", CORES_PATH
     "/gpsp_multicore_libretro.so"}, /* tzubertowski gpsp_multicore */
    {"gbav", CORES_PATH "/vba_next_libretro.so"},
    {"mgba", CORES_PATH "/mgba_libretro.so"},
    {"gbaf", CORES_PATH "/mgba_libretro.so"},
    {"GBA", CORES_PATH "/gpsp_libretro.so"},
    /* Sega */
    {"sega", CORES_PATH "/picodrive_libretro.so"},
    {"gg", CORES_PATH "/gearsystem_libretro.so"},
    {"gpgx", CORES_PATH "/genesis_plus_gx_libretro.so"},
    {"segacd", CORES_PATH
     "/genesis_plus_gx_libretro.so"}, /* Sega CD / Mega CD (needs BIOS) */
    {"MD", CORES_PATH "/picodrive_libretro.so"},
    {"32x",
     CORES_PATH "/picodrive_libretro.so"}, /* Sega 32X (heavy, may run slow) */
    {"SMS", CORES_PATH "/picodrive_libretro.so"},
    {"GG", CORES_PATH "/gearsystem_libretro.so"},
    /* Atari */
    {"a26", CORES_PATH "/stella2014_libretro.so"},
    {"a5200", CORES_PATH "/a5200_libretro.so"},
    {"a78", CORES_PATH "/prosystem_libretro.so"},
    {"a800", CORES_PATH "/atari800_libretro.so"},
    /* Lynx */
    {"lnx", CORES_PATH "/handy_libretro.so"},
    /* PC Engine */
    {"pce", CORES_PATH "/mednafen_pce_fast_libretro.so"},
    {"pcesgx", CORES_PATH "/mednafen_supergrafx_libretro.so"},
    /* Neo Geo Pocket */
    {"ngpc", CORES_PATH "/race_libretro.so"},
    /* WonderSwan */
    {"wswan", CORES_PATH "/mednafen_wswan_libretro.so"},
    {"wsv", CORES_PATH "/potator_libretro.so"},
    /* Virtual Boy */
    {"vb", CORES_PATH "/mednafen_vb_libretro.so"},
    /* PC-FX */
    {"pcfx", CORES_PATH "/mednafen_pcfx_libretro.so"},
    /* PC-8800 */
    {"pc8800", CORES_PATH "/quasi88_libretro.so"},
    /* MSX */
    {"msx", CORES_PATH "/bluemsx_libretro.so"},
    /* C64 */
    {"c64", CORES_PATH "/vice_x64_libretro.so"},
    {"c64sc", CORES_PATH
     "/vice_x64_libretro.so"}, /* only x64 built; x64sc not compiled */
    {"c64f", CORES_PATH "/frodo_libretro.so"},
    {"c64fc", CORES_PATH "/frodo_libretro.so"},
    {"vic20", CORES_PATH "/vice_xvic_libretro.so"},
    /* Amstrad */
    {"amstrad", CORES_PATH "/cap32_libretro.so"},
    {"amstradb", CORES_PATH "/cap32_libretro.so"},
    /* ZX Spectrum */
    {"spec", CORES_PATH "/fuse_libretro.so"},
    {"zx81", CORES_PATH "/81_libretro.so"},
    /* Coleco */
    {"col", CORES_PATH "/gearcoleco_libretro.so"},
    /* Ports / games */
    {"Quake", CORES_PATH "/tyrquake_libretro.so"},
    {"quake2", CORES_PATH "/vitaquake2_libretro.so"},
    {"scummvm", CORES_PATH "/scummvm_libretro.so"},
    {"outrun", CORES_PATH "/cannonball_libretro.so"},
    {"wolf3d", CORES_PATH "/ecwolf_libretro.so"},
    {"prboom", CORES_PATH "/prboom_libretro.so"},
    {"cavestory", CORES_PATH "/nxengine_libretro.so"},
    {"flashback", CORES_PATH "/reminiscence_libretro.so"},
    {"xrick", CORES_PATH "/xrick_libretro.so"},
    {"gw", CORES_PATH "/gw_libretro.so"},
    {"jnb", CORES_PATH "/jumpnbump_libretro.so"},
    /* Misc */
    {"pico8", CORES_PATH "/fake08_libretro.so"}, /* PICO-8 (fake08 core) */
    {"pico286", PICO286_BIN}, /* DOS PC (standalone, launched directly) */
    {"nds", DSPERATE_BIN},    /* Nintendo DS (DSperate standalone) */
    {"j2me", J2ME_CORE},      /* J2ME / MIDP 2.0 (.jar/.jad) */
    {"lgpt", LGPT_BIN}, /* LittleGPTracker (standalone, launched directly) */
    {"rockbox", ROCKBOX_BIN}, /* Rockbox music player (standalone) */
    {"Ebook", EBOOK_BIN},     /* ebook reader (epub/mobi/pdf, standalone) */
    {"ebook", EBOOK_BIN},
    {"videos", VIDEO_BIN}, /* MP4/MKV/AVI/etc. (standalone) */
    {"video", VIDEO_BIN},
    {"music", VIDEO_BIN}, /* folder-based simple music player */
    {"audio", VIDEO_BIN},
    {"frogshell", FROGSHELL_CORE},
    {"images", IMAGE_BIN}, /* JPG/PNG/BMP/GIF/etc. (standalone) */
    {"photos", IMAGE_BIN},
    {"fake08", CORES_PATH "/fake08_libretro.so"}, /* legacy folder name */
    {"lowres-nx", CORES_PATH "/lowresnx_libretro.so"},
    {"tic80",
     CORES_PATH "/tic80_libretro.so"}, /* TIC-80 fantasy console (.tic carts) */
    {"gme", CORES_PATH "/gme_libretro.so"},
    {"m2k", CORES_PATH "/mame2000_libretro.so"},
    {"arcade", CORES_PATH "/mame2000_libretro.so"},
    {"cps1",
     CORES_PATH "/fbalpha2012_cps1_libretro.so"}, /* Capcom CPS-1 (FBA 2012) */
    {"cps2",
     CORES_PATH "/fbalpha2012_cps2_libretro.so"}, /* Capcom CPS-2 (FBA 2012) */
    {"cps3",
     CORES_PATH "/fbalpha2012_cps3_libretro.so"}, /* Capcom CPS-3 (FBA 2012,
                                                     experimental: heavy) */
    {"neogeo",
     CORES_PATH "/fbalpha2012_neogeo_libretro.so"}, /* Neo Geo (FBA 2012) */
    {"pokem", CORES_PATH "/pokemini_libretro.so"},
    {"int", CORES_PATH "/freeintv_libretro.so"},
    {"fcf", CORES_PATH "/freechaf_libretro.so"},
    {"cdg", CORES_PATH "/pocketcdg_libretro.so"},
    {"chip8", CORES_PATH "/jaxe_libretro.so"},
    {"retro8", CORES_PATH "/retro8_libretro.so"},
    {"arduboy", CORES_PATH
     "/ardens_libretro.so"}, /* default: Ardens (fast custom AVR core) */
    {"arduous",
     CORES_PATH "/arduous_libretro.so"}, /* alt: simavr-based arduous
                                            (cycle-accurate, slower) */
    {"vec", CORES_PATH "/vecx_libretro.so"},
    {"thom", CORES_PATH "/theodore_libretro.so"},
    {"o2em", CORES_PATH "/o2em_libretro.so"},
    {"xmil", CORES_PATH "/x68k_libretro.so"},
    {"geolith", CORES_PATH "/geolith_libretro.so"},
    {"gong", CORES_PATH "/gong_libretro.so"},
    {"vapor", CORES_PATH "/vaporspec_libretro.so"},
    {"amiga", CORES_PATH "/uae_libretro.so"},
    {"atari-st", CORES_PATH "/castaway_libretro.so"},
    /* PlayStation: ps1/psx/PS run standalone PCSX4ALL (via is_ps1_folder);
     * these pcsx_rearmed entries are only the fallback if the pcsx4all binary
     * is missing.  ps1r runs the pcsx_rearmed libretro core directly. */
    {"ps1", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {"psx", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {"PS", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {"ps1r", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {"qpsx", CORES_PATH "/qpsx_libretro.so"},
    /* PSP: PPSSPP libretro core (software renderer, IR interpreter) */
    {"psp", CORES_PATH "/ppsspp_libretro.so"},
    {"PSP", CORES_PATH "/ppsspp_libretro.so"},
    {NULL, NULL}};

const char *get_core_for_folder(const char *folder) {
  if (!folder)
    return NULL;
  for (int i = 0; console_mappings[i].console_name; i++)
    if (strcasecmp(console_mappings[i].console_name, folder) == 0)
      return console_mappings[i].core_path;
  return NULL;
}

/* Extension fallback: when folder name doesn't match a console mapping,
 * pick a core based on the ROM file extension. */
typedef struct {
  const char *ext;
  const char *core_path;
} ExtensionMapping;
static const ExtensionMapping ext_mappings[] = {
    {".nes", CORES_PATH "/fceumm_libretro.so"},
    {".fds", CORES_PATH "/fceumm_libretro.so"},
    {".unf", CORES_PATH "/fceumm_libretro.so"},
    {".sfc", CORES_PATH "/snes9x2005_plus_libretro.so"},
    {".smc", CORES_PATH "/snes9x2005_plus_libretro.so"},
    {".gba", CORES_PATH "/gpsp_libretro.so"},
    {".gb", CORES_PATH "/gambatte_libretro.so"},
    {".gbc", CORES_PATH "/gambatte_libretro.so"},
    {".md", CORES_PATH "/picodrive_libretro.so"},
    {".smd", CORES_PATH "/picodrive_libretro.so"},
    {".gen", CORES_PATH "/picodrive_libretro.so"},
    {".sms", CORES_PATH "/picodrive_libretro.so"},
    {".gg", CORES_PATH "/gearsystem_libretro.so"},
    {".pce", CORES_PATH "/mednafen_pce_fast_libretro.so"},
    {".sgx", CORES_PATH "/mednafen_supergrafx_libretro.so"},
    {".lnx", CORES_PATH "/handy_libretro.so"},
    {".lyx", CORES_PATH "/handy_libretro.so"},
    {".ngp", CORES_PATH "/race_libretro.so"},
    {".ngc", CORES_PATH "/race_libretro.so"},
    {".ws", CORES_PATH "/mednafen_wswan_libretro.so"},
    {".wsc", CORES_PATH "/mednafen_wswan_libretro.so"},
    {".vb", CORES_PATH "/mednafen_vb_libretro.so"},
    {".a26", CORES_PATH "/stella2014_libretro.so"},
    {".a52", CORES_PATH "/a5200_libretro.so"},
    {".a78", CORES_PATH "/prosystem_libretro.so"},
    {".min", CORES_PATH "/pokemini_libretro.so"},
    {".col", CORES_PATH "/gearcoleco_libretro.so"},
    {".int", CORES_PATH "/freeintv_libretro.so"},
    {".bin", CORES_PATH "/freechaf_libretro.so"},
    {".sv", CORES_PATH "/potator_libretro.so"},
    {".d64", CORES_PATH "/vice_x64_libretro.so"},
    {".tap", CORES_PATH "/fuse_libretro.so"},
    {".tzx", CORES_PATH "/fuse_libretro.so"},
    {".dsk", CORES_PATH "/cap32_libretro.so"},
    {".cdt", CORES_PATH "/cap32_libretro.so"},
    {".cas", CORES_PATH "/atari800_libretro.so"},
    {".xex", CORES_PATH "/atari800_libretro.so"},
    {".atr", CORES_PATH "/atari800_libretro.so"},
    {".vec", CORES_PATH "/vecx_libretro.so"},
    {".rom", CORES_PATH "/o2em_libretro.so"},
    {".adf", CORES_PATH "/uae_libretro.so"},
    {".st", CORES_PATH "/castaway_libretro.so"},
    {".jar", J2ME_CORE},
    {".jad", J2ME_CORE},
    {".msa", CORES_PATH "/castaway_libretro.so"},
    {".cue", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {".iso", CORES_PATH "/pcsx_rearmed_libretro.so"},
    {".mp4", VIDEO_BIN},
    {".mkv", VIDEO_BIN},
    {".avi", VIDEO_BIN},
    {".mov", VIDEO_BIN},
    {".m4v", VIDEO_BIN},
    {".mpg", VIDEO_BIN},
    {".mpeg", VIDEO_BIN},
    {".ts", VIDEO_BIN},
    {".webm", VIDEO_BIN},
    {".wmv", VIDEO_BIN},
    {".mp3", VIDEO_BIN},
    {".m4a", VIDEO_BIN},
    {".aac", VIDEO_BIN},
    {".wav", VIDEO_BIN},
    {".flac", VIDEO_BIN},
    {".ogg", VIDEO_BIN},
    {".opus", VIDEO_BIN},
    {".jpg", IMAGE_BIN},
    {".jpe", IMAGE_BIN},
    {".jpeg", IMAGE_BIN},
    {".png", IMAGE_BIN},
    {".bmp", IMAGE_BIN},
    {".gif", IMAGE_BIN},
    {".tga", IMAGE_BIN},
    {".targa", IMAGE_BIN},
    {".ico", IMAGE_BIN},
    {".webp", IMAGE_BIN},
    {".tif", IMAGE_BIN},
    {".tiff", IMAGE_BIN},
    {NULL, NULL}};

const char *get_core_for_extension(const char *filename) {
  if (!filename)
    return NULL;
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return NULL;
  for (int i = 0; ext_mappings[i].ext; i++)
    if (strcasecmp(ext_mappings[i].ext, dot) == 0)
      return ext_mappings[i].core_path;
  return NULL;
}

bool is_ps1_folder(const char *folder) {
  if (!folder)
    return false;
  return strcasecmp(folder, "ps1") == 0 || strcasecmp(folder, "psx") == 0 ||
         strcasecmp(folder, "PS") == 0;
}

bool is_pico286_folder(const char *folder) {
  return folder && strcasecmp(folder, "pico286") == 0;
}

bool is_lgpt_folder(const char *folder) {
  return folder && strcasecmp(folder, "lgpt") == 0;
}

bool is_psp_folder(const char *folder) {
  return folder && strcasecmp(folder, "psp") == 0;
}

/* A standalone-launched binary (run directly, not as a libretro core). */
bool is_standalone_bin(const char *name) {
  return name &&
         (strcmp(name, PCSX4ALL_BIN) == 0 || strcmp(name, PICO286_BIN) == 0 ||
          strcmp(name, LGPT_BIN) == 0 || strcmp(name, ROCKBOX_BIN) == 0 ||
          strcmp(name, EBOOK_BIN) == 0 || strcmp(name, VIDEO_BIN) == 0 ||
          strcmp(name, IMAGE_BIN) == 0 || strcmp(name, PPSSPP_BIN) == 0 ||
          strcmp(name, DSPERATE_BIN) == 0);
}
