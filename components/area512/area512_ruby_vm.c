#include "area512_ruby_vm.h"

#include "area512_arena.h"
#include "area512_sandbox.h"
#include "picoruby.h"
#include "ports/tree_sitter_port.h"

#include <stdbool.h>
#include <stdint.h>

#include <mrubyc.h>

#include <mrb/environment_task.c>
#include <mrb/irb_task.c>

typedef struct picogems {
  const char *name;
  const uint8_t *mrb;
  void (*initializer)(mrbc_vm *vm);
  bool required;
} picogems;

extern picogems prebuilt_gems[];

void
area512_open_ruby_vm(void) {
  for (int i = 0; prebuilt_gems[i].name != NULL; i++)
    prebuilt_gems[i].required = false;

  area512_clear_sandbox_pointers();
  area512_clear_tree_sitter_allocation_byte_size();

  mrbc_init(g_area512_arena, AREA512_ARENA_BYTE_SIZE);
}

void
area512_close_ruby_vm(void) {
  struct MRBC_ALLOC_STATISTICS statistics;

  mrbc_alloc_statistics(&statistics);
  area512_record_arena_used_byte_size((int)statistics.used);

  mrbc_cleanup();
}

void
area512_run_environment_task(void) {
  mrbc_tcb *environment_tcb = mrbc_create_task(environment_task, 0);

  mrbc_set_task_name(environment_tcb, "environment_task");
  picoruby_init_require(&environment_tcb->vm);

  mrbc_run();
}

void
area512_run_irb_task(void) {
  mrbc_create_task(irb_task, 0);

  mrbc_run();
}
