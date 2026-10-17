#include "filer_window_box.h"

#include <math.h>

#include "area512_hal.h"

#define LIT_LINE_WHITE_PERCENT 70

int
area512_interpolate_linearly(int from, int to, float fraction) {
  return (int)roundf((float)from + (float)(to - from) * fraction);
}

void
area512_draw_filer_window_box(
  void *sprite,
  int left,
  int top,
  int width,
  int height
) {

  area512_sprite_fill_rect(
    sprite,
    left,
    top,
    width,
    height,
    area512_theme_background_color()
  );

  area512_sprite_rect(
    sprite,
    left,
    top,
    width,
    height,
    area512_theme_border_color()
  );
}

void
area512_draw_filer_window_lit_line(
  void *sprite,
  int left,
  int top,
  int width,
  int height,
  int lit_line_width,
  int opacity_percent
) {

  area512_sprite_blend_rect(
    sprite,
    left + width / 2 - lit_line_width / 2,
    top + height / 2 - LIT_LINE_HEIGHT / 2,
    lit_line_width,
    LIT_LINE_HEIGHT,
    area512_theme_blend_white_over_emphasis_color(LIT_LINE_WHITE_PERCENT),
    opacity_percent
  );
}
