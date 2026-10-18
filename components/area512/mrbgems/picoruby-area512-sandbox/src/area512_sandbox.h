#pragma once

#define AREA512_SANDBOX_RUN_COMPLETED 0
#define AREA512_SANDBOX_RUN_RAISED 1
#define AREA512_SANDBOX_RUN_LOAD_FAILED 2
#define AREA512_SANDBOX_RUN_INTERRUPTED 3

typedef struct sandbox_state SandboxState;

SandboxState *area512_open_sandbox(const char *task_name);

int area512_run_sandbox_mrb_file(
  SandboxState *sandbox_state,
  const char *mrb_path,
  char *exception_message,
  int exception_message_size,
  const char **exception_class_name
);

int area512_run_sandbox_manifest(
  SandboxState *sandbox_state,
  const char *directory_path,
  const char *manifest_path,
  char *exception_message,
  int exception_message_size,
  const char **exception_class_name
);

void area512_clear_sandbox_pointers(void);
void area512_terminate_sandbox_on_signal(void);
