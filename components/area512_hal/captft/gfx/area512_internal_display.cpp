#include "area512_hal.h"
#include "area512_crt.h"
#include "area512_display.h"

#include <M5Unified.hpp>
#include <lgfx/v1/LGFX_Sprite.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>

#include <pthread.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr int WAKEUP_DELAY_MS = 130;
static constexpr int SPRITE_PIXEL_BYTE_SIZE = 2;
static constexpr int REGION_BUFFER_ROW_CAPACITY =
  AREA512_SCREEN_REGION_ROW_COUNT + AREA512_CRT_RADIUS * 2;
static constexpr bool CORE_SHIFTS_GREEN_AND_NARROWS_BLUE = true;
static pthread_mutex_t internal_display_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool is_internal_display_begun;
static lgfx::LGFX_Sprite s_region_sprite(&M5.Display);
static uint8_t *s_region_buffer;
static lgfx::LovyanGFX *s_drawing_target = &M5.Display;
static int s_region_buffer_first_row;
static int s_region_destination_first_row;
static int s_region_destination_row_count;
static int s_next_region_first_row;

extern "C" int
area512_internal_display_begin(void) {
  if (!area512_display_device())
    return 0;

  pthread_mutex_lock(&internal_display_mutex);

  auto bus = static_cast<lgfx::Bus_SPI *>(M5.Display.panel()->bus());
  auto config = bus->config();

  config.dma_channel = 0;
  bus->config(config);

  if (!bus->init()) {
    pthread_mutex_unlock(&internal_display_mutex);
    return 0;
  }

  is_internal_display_begun = true;

  if (M5.Display.getBrightness() == 0) {
    M5.Display.wakeup();
    vTaskDelay(pdMS_TO_TICKS(WAKEUP_DELAY_MS));
    M5.Display.setBrightness(90);
  }

  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.setTextWrap(false, false);
  M5.Display.setTextScroll(false);
  M5.Display.clearClipRect();
  s_region_sprite.setTextSize(1);
  s_region_sprite.setTextWrap(false, false);

  return 1;
}

extern "C" void
area512_internal_display_end(void) {
  if (!is_internal_display_begun)
    return;

  M5.Display.waitDisplay();
  M5.Display.releaseBus();
  is_internal_display_begun = false;

  pthread_mutex_unlock(&internal_display_mutex);
}

extern "C" int
area512_internal_display_begin_region(void) {
  if (!is_internal_display_begun)
    return 0;

  int display_width = M5.Display.width();
  int display_height = M5.Display.height();

  if (s_next_region_first_row >= display_height) {
    s_region_sprite.deleteSprite();
    free(s_region_buffer);
    s_region_buffer = nullptr;
    s_drawing_target = &M5.Display;
    s_region_buffer_first_row = 0;
    s_next_region_first_row = 0;

    return 0;
  }

  if (s_next_region_first_row == 0 && area512_theme_is_crt_enabled())
    s_region_buffer = static_cast<uint8_t *>(malloc(
      (size_t)display_width * REGION_BUFFER_ROW_CAPACITY *
      SPRITE_PIXEL_BYTE_SIZE
    ));

  if (s_region_buffer == nullptr) {
    s_drawing_target = &M5.Display;
    s_region_buffer_first_row = 0;
    s_next_region_first_row = display_height;

    return 1;
  }

  Area512CrtRegion region;

  area512_crt_region(
    s_next_region_first_row,
    AREA512_SCREEN_REGION_ROW_COUNT,
    display_height,
    false,
    &region
  );

  s_region_sprite.setBuffer(
    s_region_buffer,
    display_width,
    region.source_count,
    16
  );

  s_drawing_target = &s_region_sprite;
  s_region_buffer_first_row = region.source_first;
  s_region_destination_first_row = region.destination_first;
  s_region_destination_row_count = region.destination_count;
  s_next_region_first_row += region.destination_count;

  return 1;
}

extern "C" void
area512_internal_display_push_region(void) {
  if (s_drawing_target != &s_region_sprite)
    return;

  area512_crt_push(
    &M5.Display,
    &s_region_sprite,
    0,
    s_region_buffer_first_row,
    CORE_SHIFTS_GREEN_AND_NARROWS_BLUE,
    false,
    0,
    s_region_destination_first_row - s_region_buffer_first_row,
    s_region_destination_row_count
  );
}

extern "C" int
area512_internal_display_width(void) {
  return M5.Display.width();
}

extern "C" int
area512_internal_display_height(void) {
  return M5.Display.height();
}

extern "C" void
area512_internal_display_fill_rect(
  int x,
  int y,
  int width,
  int height,
  uint32_t color
) {
  if (!is_internal_display_begun)
    return;

  s_drawing_target->fillRect(
    x,
    y - s_region_buffer_first_row,
    width,
    height,
    color
  );
}

extern "C" void
area512_internal_display_text(
  int x,
  int y,
  const char *text,
  int byte_length,
  int font_size,
  uint32_t foreground,
  uint32_t background
) {
  if (!is_internal_display_begun || !text || byte_length <= 0)
    return;

  switch (font_size) {
  case 10:
    s_drawing_target->setFont(&lgfx::v1::fonts::efontJA_10);
    break;
  case 12:
    s_drawing_target->setFont(&lgfx::v1::fonts::efontJA_12);
    break;
  case 14:
    s_drawing_target->setFont(&lgfx::v1::fonts::efontJA_14);
    break;
  case 16:
    s_drawing_target->setFont(&lgfx::v1::fonts::efontJA_16);
    break;
  case 24:
    s_drawing_target->setFont(&lgfx::v1::fonts::efontJA_24);
    break;
  default:
    return;
  }

  s_drawing_target->setTextColor(foreground, background);
  s_drawing_target->setCursor(x, y - s_region_buffer_first_row);
  s_drawing_target->write(text, (size_t)byte_length);
}
