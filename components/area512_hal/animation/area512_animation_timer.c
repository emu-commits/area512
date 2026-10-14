#include "area512_animation_timer.h"

#include <math.h>

#include "esp_timer.h"

int64_t
area512_read_uptime_ms(void) {
  return esp_timer_get_time() / 1000;
}

AnimationTimer
area512_start_animation_timer(int64_t duration_ms) {
  return (AnimationTimer){
    .started_at_ms = area512_read_uptime_ms(),
    .duration_ms = duration_ms,
  };
}

int64_t
area512_compute_animation_elapsed_ms(
  AnimationTimer animation_timer,
  int64_t now_ms
) {

  if (now_ms < animation_timer.started_at_ms)
    return 0;

  return now_ms - animation_timer.started_at_ms;
}

float
area512_compute_animation_progress(
  AnimationTimer animation_timer,
  int64_t now_ms
) {

  float elapsed_ms =
    (float)area512_compute_animation_elapsed_ms(animation_timer, now_ms);

  return fminf(elapsed_ms / (float)animation_timer.duration_ms, 1.0f);
}

bool
area512_is_animation_over(AnimationTimer animation_timer, int64_t now_ms) {
  return area512_compute_animation_progress(animation_timer, now_ms) >= 1.0f;
}

float
area512_compute_progress_between(
  float progress,
  float start_progress,
  float end_progress
) {
  return (progress - start_progress) / (end_progress - start_progress);
}

float
area512_compute_ease_out_progress(float progress) {
  float remaining_progress = 1.0f - fmaxf(0.0f, fminf(progress, 1.0f));

  return 1.0f - remaining_progress * remaining_progress * remaining_progress;
}
