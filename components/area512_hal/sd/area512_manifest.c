#include "area512_hal.h"

#include <stdio.h>

char *
area512_read_next_manifest_entry(char **manifest_cursor) {
  char *line_start = *manifest_cursor;

  while (*line_start) {
    char *line_end = line_start;

    while (*line_end && *line_end != '\n')
      line_end++;

    char *next_line_start = *line_end ? line_end + 1 : line_end;

    while (
      line_end > line_start &&
      (line_end[-1] == '\r' || line_end[-1] == ' ' || line_end[-1] == '\t')
    ) {

      line_end--;
    }

    *line_end = '\0';

    while (*line_start == ' ' || *line_start == '\t')
      line_start++;

    if (*line_start && *line_start != '#') {
      *manifest_cursor = next_line_start;

      return line_start;
    }

    line_start = next_line_start;
  }

  *manifest_cursor = line_start;

  return NULL;
}

void
area512_join_manifest_entry_path(
  const char *directory_path,
  const char *entry_name,
  char *entry_path,
  int entry_path_size
) {

  if (entry_name[0] == '/')
    snprintf(entry_path, entry_path_size, "%s", entry_name);
  else
    snprintf(entry_path, entry_path_size, "%s/%s", directory_path, entry_name);
}
