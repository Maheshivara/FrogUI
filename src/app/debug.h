#ifndef DEBUG_H
#define DEBUG_H
/* Logs only when /mnt/sdcard/log.txt exists (avoids FAT wear on normal boots). */
void dbg(const char *msg);
long long monotonic_ms(void);
#endif
