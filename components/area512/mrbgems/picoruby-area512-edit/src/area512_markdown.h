#ifndef AREA512_MARKDOWN_H
#define AREA512_MARKDOWN_H

#include <mrubyc.h>

void define_markdown_class(mrbc_vm *virtual_machine);
int area512_view_markdown_file(
  const char *path,
  const char **exception_message
);

#ifdef AREA512_INTERNAL_DISPLAY
int show_internal_markdown(const char *path);
void show_next_internal_markdown_page(void);
void show_previous_internal_markdown_page(void);
void hide_internal_markdown(void);
#endif

#endif
