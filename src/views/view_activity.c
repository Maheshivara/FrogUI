#include "../app/app.h"

/* internal helpers */
static void render_activity_graph(uint16_t *fb);

char activity_paths[128][MAX_PATH_LEN];
long activity_seconds[128];
long activity_runs[128];
time_t activity_last[128];
static int activity_has_dates = 0;
int activity_count = 0;
/* Activity Tracker reads the complete durable picoarch playtime ledger, so it
 * is not limited to the ten-item Recents list. */
void enter_activity_view(void) {
  FILE *f = fopen("/mnt/sdcard/frogui/play_sessions.txt", "r");
  char line[MAX_PATH_LEN + 96];
  activity_count = 0;
  activity_has_dates = 0;
  while (f && fgets(line, sizeof line, f)) {
    char *a = strchr(line, '\t');
    if (!a)
      continue;
    *a++ = '\0';
    char *b = strchr(a, '\t');
    if (!b)
      continue;
    *b++ = '\0';
    b[strcspn(b, "\r\n")] = '\0';
    time_t when = (time_t)atol(line);
    long seconds = atol(a);
    /* RTC may be unset (epoch 0); duration and append order are still valid. */
    if (when < 0 || seconds <= 0 || !b[0])
      continue;
    int found = -1;
    for (int i = 0; i < activity_count; i++)
      if (!strcmp(activity_paths[i], b)) {
        found = i;
        break;
      }
    if (found < 0 && activity_count < 128) {
      found = activity_count++;
      strncpy(activity_paths[found], b, MAX_PATH_LEN - 1);
      activity_paths[found][MAX_PATH_LEN - 1] = '\0';
    }
    if (found >= 0) {
      activity_seconds[found] += seconds;
      activity_runs[found]++;
      if (when > activity_last[found])
        activity_last[found] = when;
    }
  }
  if (f)
    fclose(f);
  /* Backward compatibility: old installations have totals but no session dates.
   */
  if (activity_count == 0) {
    f = fopen("/mnt/sdcard/frogui/playtime.txt", "r");
    while (f && fgets(line, sizeof line, f) && activity_count < 128) {
      char *tab = strchr(line, '\t');
      if (!tab)
        continue;
      *tab++ = '\0';
      tab[strcspn(tab, "\r\n")] = '\0';
      long seconds = atol(line);
      if (seconds <= 0 || !tab[0])
        continue;
      strncpy(activity_paths[activity_count], tab, MAX_PATH_LEN - 1);
      activity_paths[activity_count][MAX_PATH_LEN - 1] = '\0';
      activity_seconds[activity_count] = seconds;
      activity_runs[activity_count] = 1;
      activity_count++;
    }
    if (f)
      fclose(f);
  }
  /* Newest session first; without dates retain ledger order. */
  if (activity_has_dates || activity_count > 0) {
    for (int i = 0; i < activity_count; i++)
      if (activity_last[i] > 0)
        activity_has_dates = 1;
    if (activity_has_dates)
      for (int i = 0; i < activity_count; i++) {
        int best = i;
        for (int j = i + 1; j < activity_count; j++)
          if (activity_last[j] > activity_last[best])
            best = j;
        if (best != i) {
          char p[MAX_PATH_LEN];
          strcpy(p, activity_paths[i]);
          strcpy(activity_paths[i], activity_paths[best]);
          strcpy(activity_paths[best], p);
          long l = activity_seconds[i];
          activity_seconds[i] = activity_seconds[best];
          activity_seconds[best] = l;
          l = activity_runs[i];
          activity_runs[i] = activity_runs[best];
          activity_runs[best] = l;
          time_t d = activity_last[i];
          activity_last[i] = activity_last[best];
          activity_last[best] = d;
        }
      }
  }
  while (activity_count > entry_capacity) {
    entry_capacity =
        entry_capacity ? entry_capacity * 2 : INITIAL_ENTRIES_CAPACITY;
    DirEntry *ne = realloc(entries, (size_t)entry_capacity * sizeof(*entries));
    if (!ne)
      return;
    entries = ne;
  }
  entry_count = 0;
  for (int i = 0; i < activity_count; i++) {
    const char *base = strrchr(activity_paths[i], '/');
    base = base ? base + 1 : activity_paths[i];
    strncpy(entries[entry_count].name, base, 255);
    entries[entry_count].name[255] = '\0';
    char *dot = strrchr(entries[entry_count].name, '.');
    if (dot)
      *dot = '\0';
    entries[entry_count].is_dir = 0;
    entry_count++;
  }
  viewing_activity = true;
  viewing_apps = false;
  apps_browsing = false;
  viewing_recents = false;
  viewing_favourites = false;
  viewing_search = false;
  selected_index = 0;
  scroll_offset = 0;
}
static void render_activity_graph(uint16_t *fb) {
  if (!fb || activity_count <= 0)
    return;
  int gx = SCREEN_WIDTH * 57 / 100;
  int gy = UI_S(48);
  int gw = SCREEN_WIDTH - gx - UI_S(16);
  int row = UI_S(38), max_rows = (SCREEN_HEIGHT - gy - UI_S(42)) / row;
  if (gw < UI_S(80) || max_rows <= 0)
    return;
  if (max_rows > 6)
    max_rows = 6;
  long max_seconds = 1;
  for (int i = 0; i < activity_count && i < max_rows; i++)
    if (activity_seconds[i] > max_seconds)
      max_seconds = activity_seconds[i];
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, gx, gy - UI_S(18),
                 "HOURS PLAYED", COLOR_HEADER);
  for (int i = 0; i < activity_count && i < max_rows; i++) {
    int y = gy + i * row;
    int bw = (int)((long)(gw - UI_S(8)) * activity_seconds[i] / max_seconds);
    if (bw < UI_S(3))
      bw = UI_S(3);
    render_fill_rect(fb, gx, y, gw, UI_S(18), COLOR_LEGEND_BG);
    render_fill_rect(fb, gx, y, bw, UI_S(18), COLOR_SELECT_BG);
    char value[32];
    if (activity_seconds[i] >= 3600)
      snprintf(value, sizeof value, "%ldh %ldm", activity_seconds[i] / 3600,
               (activity_seconds[i] % 3600) / 60);
    else
      snprintf(value, sizeof value, "%ldm", activity_seconds[i] / 60);
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, gx + UI_S(4), y + UI_S(3),
                   value, COLOR_SELECT_TEXT);
  }
}
void render_activity_page(uint16_t *fb) {
  if (!fb)
    return;
  render_fill_rect(fb, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
  render_tabs(fb, MAIN_TAB_APPS, COLOR_BG);
  long total = 0, runs = 0;
  for (int i = 0; i < activity_count; i++) {
    total += activity_seconds[i];
    runs += activity_runs[i];
  }
  char summary[128];
  snprintf(summary, sizeof summary, "%d GAMES   %ld RUNS   %ldH %02ldM TOTAL",
           activity_count, runs, total / 3600, (total % 3600) / 60);
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, UI_S(48),
                 "PLAY ACTIVITY", COLOR_HEADER);
  font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, UI_S(70), summary,
                 COLOR_TEXT);
  if (activity_count <= 0) {
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, UI_S(125),
                   "Play a game to start your activity history.", COLOR_TEXT);
    return;
  }
  long max_seconds = 1;
  for (int i = 0; i < activity_count; i++)
    if (activity_seconds[i] > max_seconds)
      max_seconds = activity_seconds[i];
  int row_h = UI_S(54), first = scroll_offset;
  int visible = (SCREEN_HEIGHT - UI_S(92) - UI_S(34)) / row_h;
  if (visible < 1)
    visible = 1;
  for (int n = 0; n < visible && first + n < activity_count; n++) {
    int i = first + n, y = UI_S(94) + n * row_h;
    if (i == selected_index)
      render_fill_rect(fb, 0, y - UI_S(4), SCREEN_WIDTH, row_h - UI_S(4),
                       COLOR_LEGEND_BG);
    const char *base = strrchr(activity_paths[i], '/');
    base = base ? base + 1 : activity_paths[i];
    char name[96];
    strncpy(name, base, sizeof name - 1);
    name[sizeof name - 1] = '\0';
    char *dot = strrchr(name, '.');
    if (dot)
      *dot = '\0';
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y, name,
                   COLOR_TEXT);
    char detail[96];
    long s = activity_seconds[i];
    snprintf(detail, sizeof detail, "%ld runs   %ldh %02ldm total",
             activity_runs[i], s / 3600, (s % 3600) / 60);
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, PADDING, y + UI_S(20),
                   detail, COLOR_DISABLED);
    int bx = SCREEN_WIDTH * 52 / 100, bw = SCREEN_WIDTH - bx - PADDING;
    int fill = (int)((long)bw * s / max_seconds);
    if (fill < UI_S(3))
      fill = UI_S(3);
    render_fill_rect(fb, bx, y + UI_S(9), bw, UI_S(12), COLOR_LEGEND_BG);
    render_fill_rect(fb, bx, y + UI_S(9), fill, UI_S(12), COLOR_SELECT_BG);
  }
  if (activity_count > visible)
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH - UI_S(92),
                   SCREEN_HEIGHT - UI_S(42), "UP/DOWN", COLOR_DISABLED);
}
