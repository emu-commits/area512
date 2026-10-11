#include "area512_arena.h"

uint8_t g_area512_arena[AREA512_ARENA_BYTE_SIZE] __attribute__((aligned(8)));

static int s_recorded_arena_used_byte_size;

void
area512_record_arena_used_byte_size(int arena_used_byte_size) {
  if (arena_used_byte_size > AREA512_ARENA_BYTE_SIZE)
    arena_used_byte_size = AREA512_ARENA_BYTE_SIZE;

  s_recorded_arena_used_byte_size = arena_used_byte_size;
}

int
area512_metrics_arena_used_pct(void) {
  return s_recorded_arena_used_byte_size * 100 / AREA512_ARENA_BYTE_SIZE;
}

int
area512_metrics_arena_free_byte_size(void) {
  return AREA512_ARENA_BYTE_SIZE - s_recorded_arena_used_byte_size;
}
