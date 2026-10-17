#include "core/draw/draw.h"

#include <string.h>

void
find_changed_rows(Filer *filer, uint8_t *rows) {
  FilerDrawState *drawn = &filer->drawn;

  memset(rows, 0, SCREEN_HEIGHT);

  if (
    filer->full_redraw ||
    !drawn->valid ||
    area512_is_filer_window_open_animation_running() ||
    area512_is_filer_window_close_animation_running() ||
    !area512_theme_background_image()[0]
  ) {

    memset(rows, 1, SCREEN_HEIGHT);
    return;
  }

  // header -3/4-
  if (
    filer->index != drawn->index ||
    filer->count != drawn->count ||
    strcmp(filer->current_directory, drawn->current_directory) != 0
  )
    memset(rows, 1, HEADER_HEIGHT);

  memset(rows + HEADER_HEIGHT, 1, ROWS_VISIBLE * ROW_HEIGHT);

  if (strcmp(filer->message, drawn->message) != 0)
    memset(rows + BAR1_Y, 1, BAR_HEIGHT);
}

void
save_draw_state(Filer *filer) {
  FilerDrawState *drawn = &filer->drawn;

  drawn->index = filer->index;
  drawn->count = filer->count;

  memcpy(
    drawn->current_directory,
    filer->current_directory,
    CURRENT_DIRECTORY_MAX
  );

  memcpy(drawn->message, filer->message, MESSAGE_MAX);
}
