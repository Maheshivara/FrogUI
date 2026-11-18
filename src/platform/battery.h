#ifndef BATTERY_H
#define BATTERY_H
/* Hide cubevol's battery glyph, keeping its volume popup (no-op by default). */
void fb1_clear_battery_zone(void);
/* Wipe cubevol's whole overlay plane before handing back to the stock launcher. */
void fb1_clear_all(void);
int frogui_battery_charging(void);
int frogui_battery_pct(void);
#endif
