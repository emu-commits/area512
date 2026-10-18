#include "area512_display.h"

#include "area512_hal.h"

#include <M5Unified.hpp>
#include <lgfx/utility/pgmspace.h>
#include <lgfx/v1/LGFXBase.hpp>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "splash.h"

static lgfx::LGFX_Device *s_display = nullptr;

static uint16_t
rgb888_to_565(uint32_t color) {
  return (uint16_t)(((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) |
                    ((color >> 3) & 0x001F));
}

extern "C" void
area512_display_draw_boot_splash(void) {
  static constexpr int width = 240;
  static constexpr int height = 135;
  static constexpr int line_delay_ms = 1;

  if (!s_display || !epd_bitmap_allArray[0])
    return;

  const uint8_t *bitmap = (const uint8_t *)epd_bitmap_allArray[0];

  uint32_t set_bit_color;
  uint32_t clear_bit_color;

  area512_theme_pick_bitmap_colors(&set_bit_color, &clear_bit_color);

  int row_bytes = (width + 7) / 8;
  int copy_w = width < s_display->width() ? width : s_display->width();
  int copy_h = height < s_display->height() ? height : s_display->height();
  int src_x = width > s_display->width() ? (width - s_display->width()) / 2 : 0;
  int src_y =
    height > s_display->height() ? (height - s_display->height()) / 2 : 0;
  int dst_x = s_display->width() > width ? (s_display->width() - width) / 2 : 0;
  int dst_y =
    s_display->height() > height ? (s_display->height() - height) / 2 : 0;

  if (copy_w <= 0 || copy_h <= 0 || (src_x % 8) != 0)
    return;

  TickType_t delay = pdMS_TO_TICKS(line_delay_ms);

  if (delay == 0)
    delay = 1;

  void *line_handle = area512_sprite_new(copy_w, 1);

  if (line_handle == nullptr)
    return;

  lgfx::v1::LGFX_Sprite *line_sprite =
    static_cast<lgfx::v1::LGFX_Sprite *>(line_handle);

  for (int y = 0; y < copy_h; ++y) {
    const uint8_t *row = bitmap + (src_y + y) * row_bytes + src_x / 8;

    line_sprite->drawBitmap(
      0,
      0,
      row,
      copy_w,
      1,
      rgb888_to_565(set_bit_color),
      rgb888_to_565(clear_bit_color)
    );

    area512_sprite_push(line_handle, dst_x, dst_y + y);

    vTaskDelay(delay);
  }

  area512_sprite_delete(line_handle);

  vTaskDelay(pdMS_TO_TICKS(500));
}

extern "C" void
area512_display_init(void) {
  if (s_display)
    return;

  auto config = M5.config();
  M5.begin(config);

  s_display = &M5.Display;

  s_display->setBrightness(90);
  s_display->fillScreen(area512_theme_background_color());
}

extern "C" void *
area512_display_device(void) {
  return s_display;
}
