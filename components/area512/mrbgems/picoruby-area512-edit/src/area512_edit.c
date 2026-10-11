#if defined(PICORB_VM_MRUBYC)

#include "area512_edit.h"
#include "area512_markdown.h"
#include "core/editor.h"
#include "core/text/utf8.h"
#include "port/area512_editor_canvas.h"
#include "port/area512_editor_file.h"
#include "port/area512_editor_host.h"

#include "area512_hal.h"
#include <stdbool.h>
#include "io-console.h"
#include <mrubyc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Vim *core;
  Area512EditorCanvas canvas;
} editor_session;

static editor_session *
get_session(mrbc_value *values) {
  return *(editor_session **)values->instance->data;
}

static editor_session *
open_editor_session(
  const char *path,
  int path_byte_length,
  const char **exception_message
) {

  editor_session *session = (editor_session *)malloc(sizeof(editor_session));

  if (!session) {
    *exception_message = "failed to allocate Vim";

    return NULL;
  }

  session->core = NULL;
  session->canvas.row_sprite = NULL;
  session->canvas.cursor_sprite = NULL;

  Vim *core = (Vim *)malloc(sizeof(Vim));

  if (!core) {
    free(session);

    *exception_message = "failed to allocate Vim";

    return NULL;
  }

  session->core = core;

  int columns, rows;
  compute_editor_grid(&columns, &rows);

  vim_init(core, columns, rows);

  core->screen.highlight = highlight_visible_row_text;

  if (path && path_byte_length > 0) {
    vim_set_filepath(core, path, path_byte_length);

    VimString content;
    vim_string_init(&content);

    if (load_edit_file(path, path_byte_length, &content)) {
      vim_load_text(core, content.bytes, content.byte_length);
    }

    vim_string_free(&content);
  }

  return session;
}

static void
run_editor_session(editor_session *session) {
  Vim *core = session->core;
  session->canvas.char_width = EDIT_CHAR_WIDTH;
  session->canvas.row_height = EDIT_ROW_HEIGHT;
  session->canvas.font_size = EDIT_FONT_SIZE;

  io_raw_bang(false);
  area512_gfx_fill_screen(area512_theme_background_color());

  session->canvas.row_sprite = area512_sprite_new_with_font_size(
    area512_gfx_width(),
    EDIT_ROW_HEIGHT,
    EDIT_FONT_SIZE
  );

  session->canvas.cursor_sprite = area512_sprite_new_with_font_size(
    EDIT_CHAR_WIDTH,
    EDIT_ROW_HEIGHT,
    EDIT_FONT_SIZE
  );

  VimCanvas canvas = {
    .context = &session->canvas,
    .clear_row = clear_editor_canvas_row,
    .draw_row_text = draw_editor_canvas_row_text,
    .push_row = push_editor_canvas_row,
    .fill_row_span = fill_editor_canvas_row_span,
    .draw_row_underline = draw_editor_canvas_row_underline,
    .set_font_size = set_editor_canvas_font_size,
    .draw_cursor = draw_editor_canvas_cursor,
  };

  core->active_canvas = &canvas;

  core->screen.redraw_mode = VIM_REDRAW_ALL;
  vim_screen_refresh_if_needed(&core->screen, &canvas);

  for (;;) {
    int first_byte = area512_console_getch_block();
    if (first_byte < 0)
      continue;

    VimStatus status;

    if (first_byte == 27) {
      char sequence[2];
      int sequence_byte_length = read_escape_sequence(sequence);

      status = vim_handle_esc(core, sequence, sequence_byte_length);

    } else {
      char character[4];

      int character_byte_length = vim_utf8_byte_length((uint8_t)first_byte);

      if (character_byte_length < 1)
        character_byte_length = 1;

      if (character_byte_length > 4)
        character_byte_length = 4;

      character[0] = (char)first_byte;

      for (int i = 1; i < character_byte_length; i++) {
        int continuation_byte = area512_console_getch_block();

        if (continuation_byte < 0)
          character[i] = 0;
        else
          character[i] = (char)continuation_byte;
      }

      status =
        vim_handle_key(core, first_byte, character, character_byte_length);
    }

    if (core->screen.buffer.dirty >= VIM_DIRTY_CONTENT)
      vim_clear_diagnostics(core);

    vim_screen_refresh_if_needed(&core->screen, &canvas);

    if (status == VIM_QUIT)
      break;

    if (status == VIM_SAVE || status == VIM_SAVE_QUIT) {
      int saved = save_edit_file(core);

      vim_handle_after_save(core, saved);

      core->screen.redraw_mode = VIM_REDRAW_ALL;

      vim_screen_refresh_if_needed(&core->screen, &canvas);

      if (status == VIM_SAVE_QUIT && saved)
        break;
    }
  }

  if (session->canvas.row_sprite) {
    area512_sprite_delete(session->canvas.row_sprite);
    session->canvas.row_sprite = NULL;
  }

  if (session->canvas.cursor_sprite) {
    area512_sprite_delete(session->canvas.cursor_sprite);
    session->canvas.cursor_sprite = NULL;
  }

  core->active_canvas = NULL;

  io_cooked_bang();
}

static void
close_editor_session(editor_session *session) {
  if (session->canvas.row_sprite)
    area512_sprite_delete(session->canvas.row_sprite);

  if (session->canvas.cursor_sprite)
    area512_sprite_delete(session->canvas.cursor_sprite);

  if (session->core) {
    vim_free(session->core);
    free(session->core);
  }

  free(session);
}

int
area512_edit_file(const char *path, const char **exception_message) {
  editor_session *session =
    open_editor_session(path, (int)strlen(path), exception_message);

  if (!session)
    return -1;

  run_editor_session(session);
  close_editor_session(session);

  return 0;
}

static void
c_vim_new(mrbc_vm *virtual_machine, mrbc_value *values, int argument_count) {
  const char *path = NULL;
  int path_byte_length = 0;
  const char *exception_message = NULL;

  if (argument_count >= 1 && mrbc_type(values[1]) == MRBC_TT_STRING) {
    path = (const char *)values[1].string->data;
    path_byte_length = values[1].string->size;
  }

  editor_session *session =
    open_editor_session(path, path_byte_length, &exception_message);

  if (!session) {
    mrbc_raise(virtual_machine, MRBC_CLASS(RuntimeError), exception_message);

    return;
  }

  mrbc_value instance =
    mrbc_instance_new(virtual_machine, values->cls, sizeof(editor_session *));

  *(editor_session **)instance.instance->data = session;

  mrbc_decref(values);

  values[0] = instance;
}

static void
c_vim_start(mrbc_vm *virtual_machine, mrbc_value *values, int argument_count) {

  (void)virtual_machine;
  (void)argument_count;

  editor_session *session = get_session(values);

  if (session && session->core)
    run_editor_session(session);

  mrbc_decref(values);
  mrbc_set_nil(values);
}

static void
mrbc_vim_free(mrbc_value *self) {
  editor_session *session = *(editor_session **)self->instance->data;

  if (session)
    close_editor_session(session);
}

void
mrbc_area512_edit_init(mrbc_vm *virtual_machine) {
  mrbc_class *vim_class =
    mrbc_define_class(virtual_machine, "Vim", mrbc_class_object);

  mrbc_define_destructor(vim_class, mrbc_vim_free);
  mrbc_define_method(virtual_machine, vim_class, "new", c_vim_new);
  mrbc_define_method(virtual_machine, vim_class, "start", c_vim_start);

  define_markdown_class(virtual_machine);
}

#elif defined(PICORB_VM_MRUBY)

#include <mruby.h>

void
mrb_picoruby_area512_edit_gem_init(mrb_state *state) {
  (void)state;
}

void
mrb_picoruby_area512_edit_gem_final(mrb_state *state) {
  (void)state;
}

#endif
