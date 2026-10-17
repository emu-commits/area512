#include "core/draw/draw.h"

#include <string.h>

void
draw_all(Filer *filer) {
  if (!filer->row)
    return;

  filer->screen = area512_screen_new(FILER_FONT_SIZE);

  if (!filer->screen)
    return;

  adjust_scroll(filer);

  if (!filer->has_key_wait_timed_out)
    build_panel_info(&filer->panel_info);

  filer->panel_top_row =
    (filer->rows_visible - (filer->panel_info.count + 1)) / 2;

  if (filer->panel_top_row < 0)
    filer->panel_top_row = 0;

  uint8_t changed_rows[filer->height];

  find_changed_rows(filer, changed_rows);

  int draw_background = 1;
  int background_failed = 0;

  int changed_first_row = 0;

  while (changed_first_row < filer->height) {
    if (!changed_rows[changed_first_row]) {
      changed_first_row++;
      continue;
    }

    int changed_row_count = 0;

    while (
      changed_first_row + changed_row_count < filer->height &&
      changed_rows[changed_first_row + changed_row_count] &&
      changed_row_count < AREA512_SCREEN_REGION_ROW_COUNT
    )
      changed_row_count++;

    if (
      !area512_screen_begin_region_at(
        filer->screen,
        changed_first_row,
        changed_row_count
      )
    )
      break;

    area512_sprite_fill(filer->screen, area512_theme_background_color());

    filer->has_background_image = 0;

    if (draw_background) {
      filer->has_background_image =
        area512_screen_draw_rgb565(
          filer->screen,
          area512_theme_background_image(),
          filer->message,
          sizeof(filer->message)
        );

      draw_background = filer->has_background_image;

      if (!draw_background) {
        background_failed = 1;
        memset(
          changed_rows + filer->bar1_y,
          1,
          filer->height - filer->bar1_y
        );
      }
    }

    if (filer->has_background_image)
      area512_sprite_blend_rect(
        filer->screen,
        3,
        filer->bar1_y,
        filer->width - 6,
        filer->close_y - filer->bar1_y,
        area512_theme_background_color(),
        85
      );

    int row = 0;

    while (row < filer->rows_visible) {
      draw_entry(
        filer,
        filer->list_top + row * ROW_HEIGHT,
        filer->top + row,
        row,
        &filer->panel_info
      );

      row++;
    }

    draw_primary_action_bar(filer);
    draw_secondary_action_bar(filer);
    draw_frame(filer);
    draw_header(filer);

    if (area512_is_filer_window_open_animation_running())
      area512_draw_filer_window_open_animation(
        filer->screen,
        0,
        0,
        filer->width - 1,
        filer->height
      );
    else if (filer->terminal || filer->is_window_open)
      draw_window_box(filer->screen, 0, 0, filer->width - 1, filer->height);

    if (area512_is_filer_window_close_animation_running())
      area512_draw_filer_window_close_animation(
        filer->screen,
        0,
        0,
        filer->width - 1,
        filer->height
      );

    area512_screen_push_region(filer->screen);

    changed_first_row += changed_row_count;
  }

  area512_sprite_delete(filer->screen);

  filer->screen = 0;
  filer->full_redraw = 0;

  save_draw_state(filer);

  filer->drawn.valid = !background_failed;
}
