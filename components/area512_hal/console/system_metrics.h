#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int area512_metrics_battery_percent(void);
int area512_metrics_dram_used_pct(void);
int area512_metrics_stack_used_pct(void);
int area512_metrics_dram_free_byte_size(void);
int area512_metrics_stack_free_byte_size(void);

#ifdef __cplusplus
}
#endif
