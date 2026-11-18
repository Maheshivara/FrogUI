/* battery.c - battery ADC readout and the fb1 (cubevol overlay) housekeeping.
 */
#include "battery.h"
#include "../gfx/render.h"
#include "../settings/settings.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Hide cubevol's battery glyph while leaving its centered volume popup intact.
 * fb1 is rotated and/or double-buffered on SF3500-class devices, so physical
 * top-right is not reliably memory top-right and the active page is not always
 * page zero. Clear every memory corner on every virtual page. */
void fb1_clear_battery_zone(void) {
  /* "Stock Battery Indicator" on: leave cubevol's own glyph alone. */
  if (settings_stock_battery)
    return;
  /* Persistent mmap + cached geometry: open/mmap ONCE. The old per-frame
   * open+ioctl+mmap+munmap+close stalled the loop (visible input lag).
   * Do not latch a failed first attempt: early boot may reach FrogUI before
   * fb1 is ready, so retry later until the one-time mmap succeeds. */
  static unsigned char *mem = NULL;
  static int inited = 0;
  static size_t map_len = 0;
  static int pitch = 0, bpp = 4, vx = 0, vw = 0, vh = 0;
  static int corner_w = 0, corner_h = 0, mapped_rows = 0;
  if (!inited) {
    int fd = open("/dev/fb1", O_RDWR);
    if (fd < 0)
      return;
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) == 0 &&
        ioctl(fd, FBIOGET_FSCREENINFO, &fi) == 0 && fi.smem_len > 0) {
      void *m =
          mmap(NULL, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (m != MAP_FAILED) {
        mem = m;
        map_len = fi.smem_len;
        bpp = vi.bits_per_pixel / 8;
        if (bpp < 1)
          bpp = 4;
        pitch = fi.line_length ? (int)fi.line_length : (int)vi.xres * bpp;
        vx = vi.xoffset;
        vw = vi.xres;
        vh = vi.yres;
        mapped_rows = pitch > 0 ? (int)(map_len / (size_t)pitch) : 0;
        /* Match the custom icon's exact body+nub footprint: 26x13
         * baseline plus a 3px terminal, scaled with the UI. */
        corner_w = UI_S(29) + 30;
        corner_h = UI_S(13) + 30;
        inited = 1;
      }
    }
    close(fd); /* mmap survives close */
  }
  if (!mem || pitch <= 0 || vh <= 0 || mapped_rows <= 0)
    return;

  int right_x = vx + vw - corner_w;
  if (right_x < 0)
    right_x = 0;
  size_t want_bytes = (size_t)corner_w * bpp;
  size_t right_off = (size_t)right_x * bpp;
  size_t right_bytes = right_off < (size_t)pitch
                           ? min(want_bytes, (size_t)pitch - right_off)
                           : 0;

  /* R36SX's stock battery glyph is top-right on page zero. Do not clear
   * other corners/pages: those transparent writes survive a hard shutdown
   * and expose the stale TreeFrogUI frame under the stock logo. */
  int top_y = UI_S(10) - 15;
  if (top_y < 0)
    top_y = 0;
  for (int y = top_y; y < top_y + corner_h && y < vh; y++) {
    size_t ro = (size_t)y * pitch + right_off;
    if (right_bytes && ro + right_bytes <= map_len)
      memset(mem + ro, 0, right_bytes);
  }
}

/* Clear cubevol's entire overlay before handing control back to the stock
 * launcher.  The battery indicator is drawn on the main UI framebuffer, but
 * cubevol's fb1 plane can otherwise survive the handoff and show old corner
 * pixels over the shutdown/boot logo. */
void fb1_clear_all(void) {
  int fd = open("/dev/fb1", O_RDWR);
  if (fd < 0)
    return;
  struct fb_fix_screeninfo fi;
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) == 0 && fi.smem_len > 0) {
    void *mem =
        mmap(NULL, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem != MAP_FAILED) {
      memset(mem, 0, fi.smem_len);
      munmap(mem, fi.smem_len);
    }
  }
  close(fd);
}

/* Cached battery % for the header (re-read every ~2s; ADC changes slowly). Also
 * clears cubevol's battery corner each call so its glyph stays hidden. Called
 * by render_header (every screen, every frame). */
static int raw_to_pct(int raw) {
  /* This ADC is too noisy and device-dependent for a truthful percentage.
   * Use three broad states; give the full state a deliberately wide range. */
  if (raw < 100)
    return 25;
  if (raw < 145)
    return 50;
  return 100;
}
/* Persistent ADC fds, opened O_RDWR ONCE like cubevol (battery_adc_init) -
 * these nodes are flaky when re-opened per poll, especially check_adc2. slot
 * 0=adc1 (battery), 1=adc2 (charge). Read 1 byte from the kept-open fd each
 * poll. */
static int read_adc(int slot) {
  static int fd[2] = {-2, -2};
  static const char *node[2] = {"/dev/check_adc1", "/dev/check_adc5"};
  if (slot < 0 || slot > 1)
    return -1;
  if (fd[slot] == -2)
    fd[slot] = open(node[slot], O_RDWR);
  if (fd[slot] < 0)
    return -1;
  unsigned char b = 0;
  lseek(fd[slot], 0, SEEK_SET);
  int n = read(fd[slot], &b, 1);
  return (n == 1) ? b : -1;
}

static int g_batt_charging = 0;
int frogui_battery_charging(void) { return g_batt_charging; }
int frogui_battery_color_mode(void); /* defined after settings_battery_color */
int frogui_battery_pct(void) {
  /* "Stock Battery Indicator" on: -1 makes render_battery_* draw nothing, so
   * only cubevol's stock glyph is visible. Read every frame, so the setting
   * takes effect immediately (cubevol repaints its glyph on its own cycle). */
  if (settings_stock_battery)
    return -1;
  static int cached = -1, tick = 0;
  if (cached < 0 || (tick++ % 300) == 0) {
    int a1 = read_adc(0), a5 = read_adc(1);
    cached = (a1 >= 0) ? raw_to_pct(a1) : -1;
    g_batt_charging = (a5 >= 64) ? 1 : 0;
  }
  fb1_clear_battery_zone();
  return cached;
}
