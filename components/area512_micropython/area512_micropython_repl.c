#include "area512_micropython.h"
#include "area512_micropython_gc_heap.h"
#include "port/micropython_embed.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/lexer.h"
#include "py/mpprint.h"
#include "py/repl.h"
#include "py/runtime.h"

#include <string.h>

void
area512_micropython_open_repl_runtime(
  void *micropython_stack_top,
  void *gc_heap,
  size_t gc_heap_byte_size
) {

  mp_embed_init(gc_heap, gc_heap_byte_size, micropython_stack_top);
}

void
area512_micropython_exec_repl_source(const char *python_source) {
  nlr_buf_t nlr_buffer;

  if (nlr_push(&nlr_buffer) != 0) {
    mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr_buffer.ret_val);

    return;
  }

  mp_lexer_t *lexer =
    mp_lexer_new_from_str_len(
      MP_QSTR__lt_stdin_gt_,
      python_source,
      strlen(python_source),
      0
    );

  qstr python_source_name = lexer->source_name;

  mp_parse_tree_t parse_tree = mp_parse(lexer, MP_PARSE_SINGLE_INPUT);

  mp_obj_t module_function = mp_compile(&parse_tree, python_source_name, true);

  mp_call_function_0(module_function);

  nlr_pop();
}

int
area512_micropython_repl_source_is_incomplete(const char *python_source) {
  return mp_repl_continue_with_input(python_source) ? 1 : 0;
}

void
area512_micropython_close_repl_runtime(void) {
  area512_micropython_record_gc_heap_used_byte_size();
  gc_sweep_all();
  mp_embed_deinit();
  area512_micropython_free_gc_heap_split_areas();
}
