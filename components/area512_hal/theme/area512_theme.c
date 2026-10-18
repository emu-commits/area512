#include "area512_hal.h"

#include <stdio.h>
#include <string.h>

#define THEME_PATH "etc/theme"
#define THEME_LINE_BYTES 64
#define WHITE_COLOR 0xFFFFFF

typedef struct {
  char background_image[THEME_LINE_BYTES];
  uint32_t background_color;
  uint32_t text_color;
  uint32_t emphasis_color;
  uint32_t border_color;
  uint32_t selected_color;
  uint32_t box_color;
  bool is_crt_enabled;
} Theme;

static Theme s_theme = {
  .background_color = 0x000000,
  .text_color = 0xCFA45F,
  .emphasis_color = 0xF5972D,
  .border_color = 0xF5972D,
  .selected_color = 0xFFD966,
  .box_color = 0x241604,
  .is_crt_enabled = false,
};

static uint32_t
luminance(uint32_t color) {
  return 2 * ((color >> 16) & 0xFF) + 5 * ((color >> 8) & 0xFF) + (color & 0xFF);
}

static uint32_t
blend_color_channel(
  int under_channel,
  int over_channel,
  int over_color_percent
) {

  return (uint32_t)(
    under_channel + (over_channel - under_channel) * over_color_percent / 100
  );
}

static uint32_t
blend_color(uint32_t under_color, uint32_t over_color, int over_color_percent) {
  return
    (blend_color_channel(
      (under_color >> 16) & 0xFF,
      (over_color >> 16) & 0xFF,
      over_color_percent
    ) << 16) |
    (blend_color_channel(
      (under_color >> 8) & 0xFF,
      (over_color >> 8) & 0xFF,
      over_color_percent
    ) << 8) |
    blend_color_channel(
      under_color & 0xFF,
      over_color & 0xFF,
      over_color_percent
    );
}

static int
read_text_line(FILE *file, char *line, size_t line_bytes) {
  int byte_count = 0;
  int character;

  while ((character = fgetc(file)) != EOF) {
    if (character == '\n')
      break;

    if (character != '\r' && (size_t)byte_count + 1 < line_bytes)
      line[byte_count++] = (char)character;
  }

  line[byte_count] = '\0';

  return byte_count > 0 || character != EOF;
}

static int
parse_hex_color(const char *hex, uint32_t *color) {
  if (hex[0] != '0' || (hex[1] != 'x' && hex[1] != 'X'))
    return 0;

  const char *cursor = hex + 2;
  uint32_t parsed_color = 0;
  int digit_count = 0;

  while (*cursor) {
    int digit;

    if (*cursor >= '0' && *cursor <= '9')
      digit = *cursor - '0';
    else if (*cursor >= 'a' && *cursor <= 'f')
      digit = *cursor - 'a' + 10;
    else if (*cursor >= 'A' && *cursor <= 'F')
      digit = *cursor - 'A' + 10;
    else
      return 0;

    parsed_color = parsed_color * 16 + (uint32_t)digit;
    digit_count++;
    cursor++;
  }

  if (digit_count != 6)
    return 0;

  *color = parsed_color;

  return 1;
}

static void
assign_theme_color(const char *key, uint32_t color) {
  if (strcmp(key, "background") == 0)
    s_theme.background_color = color;
  else if (strcmp(key, "text") == 0)
    s_theme.text_color = color;
  else if (strcmp(key, "emphasis") == 0)
    s_theme.emphasis_color = color;
  else if (strcmp(key, "border") == 0)
    s_theme.border_color = color;
  else if (strcmp(key, "selected") == 0)
    s_theme.selected_color = color;
  else if (strcmp(key, "box") == 0)
    s_theme.box_color = color;
}

static void
assign_theme_from_line(char *line) {
  char *equal_sign = strchr(line, '=');

  if (equal_sign == NULL)
    return;

  *equal_sign = '\0';

  if (strcmp(line, "background_image") == 0) {
    snprintf(
      s_theme.background_image,
      sizeof(s_theme.background_image),
      "%s",
      equal_sign + 1
    );
    return;
  }

  if (strcmp(line, "crt") == 0) {
    if (strcmp(equal_sign + 1, "on") == 0)
      s_theme.is_crt_enabled = true;
    else if (strcmp(equal_sign + 1, "off") == 0)
      s_theme.is_crt_enabled = false;
    return;
  }

  uint32_t color;

  if (parse_hex_color(equal_sign + 1, &color))
    assign_theme_color(line, color);
}

void
area512_theme_load(void) {
  char resolved_path[AREA512_PATH_MAX];

  if (
    area512_resolve_data_path(
      THEME_PATH,
      resolved_path,
      sizeof(resolved_path)
    ) < 0
  )
    return;

  FILE *file = fopen(resolved_path, "r");

  if (file == NULL)
    return;

  char line[THEME_LINE_BYTES];

  while (read_text_line(file, line, sizeof(line)))
    assign_theme_from_line(line);

  fclose(file);
}

uint32_t
area512_theme_background_color(void) {
  return s_theme.background_color;
}

const char *
area512_theme_background_image(void) {
  return s_theme.background_image;
}

uint32_t
area512_theme_text_color(void) {
  return s_theme.text_color;
}

uint32_t
area512_theme_emphasis_color(void) {
  return s_theme.emphasis_color;
}

uint32_t
area512_theme_border_color(void) {
  return s_theme.border_color;
}

uint32_t
area512_theme_selected_color(void) {
  return s_theme.selected_color;
}

uint32_t
area512_theme_box_color(void) {
  return s_theme.box_color;
}

bool
area512_theme_is_crt_enabled(void) {
  return s_theme.is_crt_enabled;
}

uint32_t
area512_theme_blend_text_color_over_background(int text_color_percent) {
  return blend_color(
    s_theme.background_color,
    s_theme.text_color,
    text_color_percent
  );
}

uint32_t
area512_theme_blend_selected_color_over_background(int selected_color_percent) {
  return blend_color(
    s_theme.background_color,
    s_theme.selected_color,
    selected_color_percent
  );
}

uint32_t
area512_theme_blend_white_over_emphasis_color(int white_percent) {
  return blend_color(s_theme.emphasis_color, WHITE_COLOR, white_percent);
}

// Bitmaps are authored with the set bits as the brighter side, so a light
// theme has to take the emphasis color as the clear side to avoid inverting.
void
area512_theme_pick_bitmap_colors(
  uint32_t *set_bit_color,
  uint32_t *clear_bit_color
) {

  *set_bit_color = s_theme.emphasis_color;
  *clear_bit_color = s_theme.background_color;

  if (luminance(*clear_bit_color) > luminance(*set_bit_color)) {
    uint32_t swapped = *set_bit_color;

    *set_bit_color = *clear_bit_color;
    *clear_bit_color = swapped;
  }
}
