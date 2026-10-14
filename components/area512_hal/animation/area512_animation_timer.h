#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int64_t started_at_ms;
  int64_t duration_ms;
} AnimationTimer;

int64_t area512_read_uptime_ms(void);
AnimationTimer area512_start_animation_timer(int64_t duration_ms);
int64_t area512_compute_animation_elapsed_ms(
  AnimationTimer animation_timer,
  int64_t now_ms
);
float area512_compute_animation_progress(
  AnimationTimer animation_timer,
  int64_t now_ms
);
bool area512_is_animation_over(AnimationTimer animation_timer, int64_t now_ms);
float area512_compute_progress_between(
  float progress,
  float start_progress,
  float end_progress
);
float area512_compute_ease_out_progress(float progress);
