#pragma once

#include "area512_hal.h"
#include "core/filer.h"

#define PANEL_TITLE "SYS 0x200"

#define WINDOW_LEFT ((SCREEN_WIDTH - AREA512_WINDOW_WIDTH) / 2)
#define WINDOW_TOP ((FRAME_TOP + BAR1_Y - AREA512_WINDOW_HEIGHT) / 2)

void find_changed_rows(Filer *filer, uint8_t *rows);
void save_draw_state(Filer *filer);

void draw_frame(Filer *filer);
void draw_header(Filer *filer);

void build_panel_info(PanelInfo *panel_information);
void draw_panel_row(Filer *filer, int row, const PanelInfo *panel_information);
int panel_covers_row(int row);

void adjust_scroll(Filer *filer);
void draw_entry(Filer *filer, int y, int index, int row, const PanelInfo *info);

void draw_primary_action_bar(Filer *filer);
void draw_secondary_action_bar(Filer *filer);
