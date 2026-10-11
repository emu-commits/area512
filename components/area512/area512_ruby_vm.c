#include "area512_ruby_vm.h"

#include "area512_sandbox.h"
#include "picoruby.h"

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
