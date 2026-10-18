#pragma once

#include <lgfx/v1/LGFXBase.hpp>
#include <lgfx/v1/LGFX_Sprite.hpp>

static constexpr int AREA512_CRT_RADIUS = 2;

struct Area512CrtRegion {
  int source_first;
  int source_count;
  int destination_first;
  int destination_count;
};

// Changed rows also invalidate their surrounding glow. Source rows include
// another halo beyond the destination so partial updates equal a full redraw.
bool area512_crt_region(
  int first,
  int count,
  int height,
  bool changed,
  Area512CrtRegion *region
);

// Filter only the transfer; sprite contents remain available for compositing.
void area512_crt_push(
  lgfx::LGFX_Device *display,
  lgfx::LGFX_Sprite *sprite,
  int x,
  int y,
  bool transparent = false,
  uint32_t transparent_color = 0,
  int first_row = 0,
  int row_count = -1
);
