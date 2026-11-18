/* debug.c - opt-in diagnostics log and monotonic clock. */
#include "debug.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void dbg(const char *msg) {
  /* Mirror zhijack's opt-in diagnostics convention.  Writing every UI
   * startup step to FAT on normal boots is needless wear and noise. */
  static int enabled = -1;
  if (enabled < 0)
    enabled = access("/mnt/sdcard/log.txt", F_OK) == 0;
  if (!enabled)
    return;
  FILE *f = fopen("/mnt/sdcard/frogui_crash.log", "a");
  if (f) {
    fputs(msg, f);
    fputs("\n", f);
    fclose(f);
  }
  fprintf(stderr, "FROGUI_DBG: %s\n", msg);
}

long long monotonic_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
