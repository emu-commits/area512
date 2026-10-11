#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define AREA512_SHARED_STACK_BYTE_SIZE (32 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

void area512_start_replaceable_task(
  const char *task_name,
  TaskFunction_t task_function,
  void *task_parameter
);

void area512_replace_current_task(
  const char *task_name,
  TaskFunction_t task_function,
  void *task_parameter
);

#ifdef __cplusplus
}
#endif
