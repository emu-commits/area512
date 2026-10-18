#include "area512_hal.h"

#include "area512_animation_timer.h"

#define CURSOR_BLINK_PERIOD_MS 1250

static AnimationTimer s_cursor_animation_timer = {
  .duration_ms = CURSOR_BLINK_PERIOD_MS,
};

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

  if (
    !area512_is_in_first_half_of_animation_cycle(
      s_cursor_animation_timer,
      area512_read_uptime_ms()
    )
  )
    return;

  area512_sprite_text(sprite, x, y, ">", color);
}
