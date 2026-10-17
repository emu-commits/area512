#include "core/draw/draw.h"

#include <string.h>

void
find_changed_rows(Filer *filer, uint8_t *rows) {
  FilerDrawState *drawn = &filer->drawn;

  memset(rows, 0, filer->height);

  if (
    filer->full_redraw ||
    !drawn->valid ||
    filer->is_window_open ||
    area512_is_filer_window_open_animation_running() ||
    area512_is_filer_window_close_animation_running() ||
    !area512_theme_background_image()[0]
  ) {

    memset(rows, 1, filer->height);
    return;
  }

  if (
    filer->index != drawn->index ||
    filer->count != drawn->count ||
    strcmp(filer->current_directory, drawn->current_directory) != 0
  )
    memset(rows, 1, filer->list_top);

  memset(rows + filer->list_top, 1, filer->rows_visible * ROW_HEIGHT);

  if (strcmp(filer->message, drawn->message) != 0)
    memset(rows + filer->bar1_y, 1, ROW_HEIGHT);
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
