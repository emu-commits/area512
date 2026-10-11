#include "tree_sitter_port.h"
#include "micropython_ti_parse_budget.h"
#include <mrubyc.h>
#include <tree_sitter/api.h>

#define MRUBYC_POOL_RESERVE_BYTE_SIZE (16 * 1024)

static size_t live_allocation_byte_size;
static size_t parse_allocation_limit_byte_size;

static void *
allocate_from_mrubyc_pool(size_t byte_size) {
  void *allocation = mrbc_raw_alloc((unsigned int)byte_size);

  if (allocation)
    live_allocation_byte_size += mrbc_alloc_usable_size(allocation);

  return allocation;
}

static void *
allocate_zeroed_from_mrubyc_pool(
  size_t element_count,
  size_t element_byte_size
) {

  void *allocation =
    mrbc_raw_calloc(
      (unsigned int)element_count,
      (unsigned int)element_byte_size
    );

  if (allocation)
    live_allocation_byte_size += mrbc_alloc_usable_size(allocation);

  return allocation;
}

static void *
reallocate_from_mrubyc_pool(void *allocation, size_t byte_size) {
  size_t previous_usable_byte_size = 0;

  if (allocation)
    previous_usable_byte_size = mrbc_alloc_usable_size(allocation);

  void *new_allocation = mrbc_raw_realloc(allocation, (unsigned int)byte_size);

  live_allocation_byte_size -= previous_usable_byte_size;

  if (new_allocation)
    live_allocation_byte_size += mrbc_alloc_usable_size(new_allocation);

  return new_allocation;
}

static void
free_to_mrubyc_pool(void *allocation) {
  if (!allocation)
    return;

  live_allocation_byte_size -= mrbc_alloc_usable_size(allocation);

  mrbc_raw_free(allocation);
}

static void
start_parse_budget(void) {
  struct MRBC_ALLOC_STATISTICS statistics;

  mrbc_alloc_statistics(&statistics);

  parse_allocation_limit_byte_size = live_allocation_byte_size;

  if (statistics.free > MRUBYC_POOL_RESERVE_BYTE_SIZE)
    parse_allocation_limit_byte_size +=
      statistics.free - MRUBYC_POOL_RESERVE_BYTE_SIZE;
}

static int
is_parse_budget_exhausted(void) {
  return live_allocation_byte_size > parse_allocation_limit_byte_size;
}

void
area512_route_tree_sitter_allocations_to_mrubyc_pool(void) {
  ts_set_allocator(
    allocate_from_mrubyc_pool,
    allocate_zeroed_from_mrubyc_pool,
    reallocate_from_mrubyc_pool,
    free_to_mrubyc_pool
  );
}

void
area512_register_tree_sitter_parse_budget(void) {
  MicroPythonTiParseBudget parse_budget = {
    start_parse_budget,
    is_parse_budget_exhausted
  };

  micropython_ti_set_parse_budget(parse_budget);
}
