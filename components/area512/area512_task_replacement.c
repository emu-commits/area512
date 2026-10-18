#include "area512_task_replacement.h"

#include <freertos/timers.h>

#define SHARED_STACK_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define SHARED_STACK_TASK_CORE_ID 0

static StackType_t s_shared_stack[AREA512_SHARED_STACK_BYTE_SIZE];
static StaticTask_t s_shared_stack_task_buffer;
static TaskHandle_t s_shared_stack_task;
static const char *s_next_task_name;
static TaskFunction_t s_next_task_function;
static void *s_next_task_parameter;

void
area512_start_replaceable_task(
  const char *task_name,
  TaskFunction_t task_function,
  void *task_parameter
) {

  s_shared_stack_task =
    xTaskCreateStaticPinnedToCore(
      task_function,
      task_name,
      AREA512_SHARED_STACK_BYTE_SIZE,
      task_parameter,
      SHARED_STACK_TASK_PRIORITY,
      s_shared_stack,
      &s_shared_stack_task_buffer,
      SHARED_STACK_TASK_CORE_ID
    );
}

static void
replace_shared_stack_task(void *pended_parameter, uint32_t pended_value) {
  (void)pended_parameter;
  (void)pended_value;

  while (
    eTaskGetState(s_shared_stack_task) != eSuspended ||
    xTaskGetCurrentTaskHandleForCore(SHARED_STACK_TASK_CORE_ID) ==
      s_shared_stack_task
  ) {

    vTaskDelay(1);
  }

  vTaskDelete(s_shared_stack_task);

  area512_start_replaceable_task(
    s_next_task_name,
    s_next_task_function,
    s_next_task_parameter
  );
}

void
area512_replace_current_task(
  const char *task_name,
  TaskFunction_t task_function,
  void *task_parameter
) {

  s_next_task_name = task_name;
  s_next_task_function = task_function;
  s_next_task_parameter = task_parameter;

  xTimerPendFunctionCall(
    replace_shared_stack_task,
    NULL,
    0,
    portMAX_DELAY
  );

  vTaskSuspend(NULL);
}
