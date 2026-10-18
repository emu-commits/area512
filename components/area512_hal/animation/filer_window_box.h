#pragma once

#define LIT_LINE_HEIGHT 2

int area512_interpolate_linearly(int from, int to, float fraction);

void area512_draw_filer_window_box(
  void *sprite,
  int left,
  int top,
  int width,
  int height
);

void area512_draw_filer_window_lit_line(
  void *sprite,
  int left,
  int top,
  int width,
  int height,
  int lit_line_width,
  int opacity_percent
);
