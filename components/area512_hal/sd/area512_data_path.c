#include "area512_hal.h"

#include <string.h>

static int
has_parent_reference(const char *path) {
  while (*path) {
    const char *component = path;

    while (*path && *path != '/')
      path++;

    if (path - component == 2 && component[0] == '.' && component[1] == '.')
      return 1;

    while (*path == '/')
      path++;
  }

  return 0;
}

int
area512_resolve_data_path(
  const char *path,
  char *resolved_path,
  size_t resolved_path_size
) {
  while (*path == '/')
    path++;

  // path traversal guard
  if (has_parent_reference(path))
    return -1;

  size_t area512_root_path_length = strlen(AREA512_ROOT_PATH);
  size_t path_length = strlen(path);
  size_t resolved_path_length =
    area512_root_path_length + (path_length > 0 ? 1 + path_length : 0);

  if (resolved_path_length + 1 > resolved_path_size)
    return -1;

  memcpy(resolved_path, AREA512_ROOT_PATH, area512_root_path_length);

  if (path_length > 0) {
    resolved_path[area512_root_path_length] = '/';
    memcpy(
      resolved_path + area512_root_path_length + 1,
      path,
      path_length + 1
    );
  } else {
    resolved_path[area512_root_path_length] = '\0';
  }

  return 0;
}
