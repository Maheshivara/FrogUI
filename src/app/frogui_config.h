/* frogui_config.h - build-time paths/binaries shared by every FrogUI module. */
#ifndef FROGUI_CONFIG_H
#define FROGUI_CONFIG_H

#define SDCARD_BASE "/mnt/sdcard"
#define CORES_PATH SDCARD_BASE "/cubegm/cores"
/* Core audio rate (single source of truth: av_info AND the menu-tick length
 * are both derived from this - see ui_menu_tick). */
#define SAMPLE_RATE 44100
/* roms root: resolved at init. Prefer roms/, accept ROMS/ (exfat-mounted cards
 * are case-sensitive on this kernel), create roms/ if neither exists, so the
 * menu ALWAYS has a valid root instead of NULL entries → SIGBUS addr=0. */
#define ROMS_PATH_DEFAULT SDCARD_BASE "/roms"
#define OTG_MOUNT_PATH "/media/hdd"
#define OTG_ROMS_PATH OTG_MOUNT_PATH "/roms"
/* The launcher may point FrogUI at a mounted USB disk. */
extern char g_roms_path[512];
#define ROMS_PATH g_roms_path
#define LAUNCH_FILE "/tmp/frogui_launch.txt"
#define LANGUAGE_RESTART_FLAG SDCARD_BASE "/frogui/language_restart.flag"
#define PCSX4ALL_BIN SDCARD_BASE "/cubegm/pcsx4all"
#define PICO286_BIN SDCARD_BASE "/cubegm/pico286"
#define LGPT_BIN SDCARD_BASE "/cubegm/lgpt"
#define ROCKBOX_BIN                                                            \
  SDCARD_BASE "/cubegm/rockbox.sh" /* wrapper sets HOME+SDL env */
#define EBOOK_BIN                                                              \
  SDCARD_BASE "/cubegm/ebook" /* MuPDF ebook reader (epub/mobi/pdf) */
#define VIDEO_BIN                                                              \
  SDCARD_BASE "/cubegm/video_player" /* hardware-decoded video player */
#define IMAGE_BIN                                                              \
  SDCARD_BASE "/cubegm/image_viewer" /* hardware-decoded image viewer */
#define PPSSPP_BIN                                                             \
  SDCARD_BASE "/cubegm/ppsspp" /* optional standalone SF3000 port */
#define DSPERATE_BIN SDCARD_BASE "/cubegm/dsperate/run_sf3000.sh"
#define J2ME_CORE CORES_PATH "/j2me_libretro.so"
#define FROGSHELL_CORE                                                         \
  CORES_PATH "/frogshell_libretro.so" /* file manager via picoarch */
#define USB_MODE_BIN                                                           \
  SDCARD_BASE "/cubegm/usb_mtp.sh" /* expose the SD card to a USB host */
#define SHUTDOWN_BIN                                                           \
  SDCARD_BASE "/cubegm/shutdown.sh" /* power off the console */

#endif
