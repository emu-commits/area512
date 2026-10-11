#include "area512_display.h"

#include "area512_hal.h"

#if !defined(AREA512_TDECK)
#include <M5Unified.hpp>
#endif
#include <lgfx/utility/pgmspace.h>
#include <lgfx/v1/LGFXBase.hpp>
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/platforms/esp32/Light_PWM.hpp>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Panel_ILI9341.hpp"
#include "splash.h"

static constexpr int PANEL_WIDTH = 240;
static constexpr int PANEL_HEIGHT = 320;
static constexpr int PANEL_ROTATION = 1;
static constexpr int SPLASH_LINE_DELAY_MS = 1;
static constexpr int SPLASH_HOLD_MS = 500;

#if defined(AREA512_TDECK)
static constexpr int PIN_SCLK = 40;
static constexpr int PIN_MOSI = 41;
static constexpr int PIN_MISO = 38;
static constexpr int PIN_DC = 11;
static constexpr int PIN_CS = 12;
static constexpr int PIN_RST = -1;
static constexpr int PIN_BACKLIGHT = 42;
static constexpr uint32_t BACKLIGHT_PWM_FREQUENCY = 44100;
static constexpr bool PANEL_INVERT = true;

static constexpr gpio_num_t TDECK_POWER_ON = GPIO_NUM_10;
static constexpr gpio_num_t TDECK_SD_CS = GPIO_NUM_39;
static constexpr gpio_num_t TDECK_RADIO_CS = GPIO_NUM_9;
static constexpr int TDECK_POWER_ON_DELAY_MS = 100;
#else
static constexpr int PIN_SCLK = 40;
static constexpr int PIN_MOSI = 14;
static constexpr int PIN_MISO = 39;
static constexpr int PIN_DC = 6;
static constexpr int PIN_CS = 5;
static constexpr int PIN_RST = 3;
static constexpr bool PANEL_INVERT = false;
#endif

static lgfx::Bus_SPI s_bus;
#if defined(AREA512_CAPTFT_ILI9341)
static lgfx::Panel_ILI9341 s_panel;
#else
static lgfx::Panel_ST7789 s_panel;
#endif
#if defined(AREA512_TDECK)
static lgfx::Light_PWM s_light;
#endif
static lgfx::LGFX_Device s_external_display;
static lgfx::LGFX_Device *s_display = nullptr;

static void
configure_panel(void) {
  auto bus_config = s_bus.config();

  // SPI2: shared by the display and the SD card (area512_sd.c).
  bus_config.spi_host = SPI2_HOST;
  bus_config.pin_sclk = PIN_SCLK;
  bus_config.pin_mosi = PIN_MOSI;
  bus_config.use_lock = true;

  // SPI2: SD card only. The display has no MISO, but SD mount reuses this init.
  bus_config.pin_miso = PIN_MISO;

  // SPI2: display only.
  bus_config.spi_mode = 0;
#if defined(AREA512_CAPTFT_ILI9341)
  bus_config.freq_write = 20000000;
#else
  bus_config.freq_write = 40000000;
#endif
  bus_config.spi_3wire = false;
  bus_config.pin_dc = PIN_DC;

  s_bus.config(bus_config);
  s_panel.setBus(&s_bus);

  // Display panel.
  auto config = s_panel.config();
  config.pin_cs = PIN_CS;
  config.pin_rst = PIN_RST;
  config.memory_width = PANEL_WIDTH;
  config.memory_height = PANEL_HEIGHT;
  config.panel_width = PANEL_WIDTH;
  config.panel_height = PANEL_HEIGHT;
  config.offset_x = 0;
  config.offset_y = 0;

#if defined(AREA512_CAPTFT_ILI9341)
  config.offset_rotation = 2;
#endif
  config.invert = PANEL_INVERT;
  config.rgb_order = false;
  config.readable = false;
  config.bus_shared = true;
  s_panel.config(config);

#if defined(AREA512_TDECK)
  auto light_config = s_light.config();

  light_config.pin_bl = PIN_BACKLIGHT;
  light_config.freq = BACKLIGHT_PWM_FREQUENCY;
  s_light.config(light_config);
  s_panel.setLight(&s_light);
#endif

  s_external_display.setPanel(&s_panel);
}

extern "C" void
area512_display_draw_boot_splash(void) {
  if (!s_display || !epd_bitmap_allArray[0])
    return;

  area512_gfx_show_bitmap(
    epd_bitmap_allArray[0],
    SPLASH_LINE_DELAY_MS
  );

  vTaskDelay(pdMS_TO_TICKS(SPLASH_HOLD_MS));
}

extern "C" void
area512_display_init(void) {
  if (s_display)
    return;

#if defined(AREA512_TDECK)
  // Power the display, keyboard and SD card, then deselect the SD card and
  // radio before initializing the display on their shared bus.
  gpio_config_t gpio_output_config = {};

  gpio_output_config.pin_bit_mask = (1ULL << TDECK_POWER_ON) |
                                    (1ULL << TDECK_SD_CS) |
                                    (1ULL << TDECK_RADIO_CS);
  gpio_output_config.mode = GPIO_MODE_OUTPUT;
  gpio_config(&gpio_output_config);

  gpio_set_level(TDECK_POWER_ON, 1);
  gpio_set_level(TDECK_SD_CS, 1);
  gpio_set_level(TDECK_RADIO_CS, 1);
  vTaskDelay(pdMS_TO_TICKS(TDECK_POWER_ON_DELAY_MS));
#else
  auto config = M5.config();
  M5.begin(config);

  // Keep M5 board/peripheral initialization, then release the internal display.
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  M5.Display.waitDisplay();
  M5.Display.releaseBus();

  // Deselect the SD card before initializing the display on their shared bus.
  gpio_set_level(GPIO_NUM_12, 1);
  gpio_set_direction(GPIO_NUM_12, GPIO_MODE_OUTPUT);
#endif

  configure_panel();
  s_external_display.init();
  s_display = &s_external_display;
  s_display->setColorDepth(16);
  s_display->setRotation(PANEL_ROTATION);

  s_display->fillScreen(area512_theme_background_color());
}

extern "C" void *
area512_display_device(void) {
  return s_display;
}
