#include "area512_hal.h"

#include "area512_animation_timer.h"

#define CURSOR_BLINK_PERIOD_MS 1250

static AnimationTimer s_cursor_animation_timer = {
  .duration_ms = CURSOR_BLINK_PERIOD_MS,
};

static bool
is_filer_cursor_shown(int64_t now_ms) {
  int64_t elapsed_ms =
    area512_compute_animation_elapsed_ms(s_cursor_animation_timer, now_ms);
  int64_t blink_period_ms = s_cursor_animation_timer.duration_ms;

  return elapsed_ms % blink_period_ms < blink_period_ms / 2;
}

void
area512_start_filer_cursor_animation(void) {
  s_cursor_animation_timer =
    area512_start_animation_timer(CURSOR_BLINK_PERIOD_MS);
}

void
area512_draw_filer_cursor_animation(
  void *sprite,
  int x,
  int y,
  uint32_t color
) {

  if (!is_filer_cursor_shown(area512_read_uptime_ms()))
    return;

  area512_sprite_text(sprite, x, y, ">", color);
}
