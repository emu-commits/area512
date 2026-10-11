#include "filer_shell.h"

#include "area512_arena.h"
#include "area512_compile.h"
#include "area512_dot.h"
#include "area512_edit.h"
#include "area512_hal.h"
#include "area512_markdown.h"
#include "area512_micropython.h"
#include "area512_ruby_vm.h"
#include "area512_sandbox.h"
#include "area512_shared_stack.h"
#include "python_repl.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <esp_system.h>
#include <mrubyc.h>

// filer.h reuses these libc limit names for its own buffer sizes.
#undef LINE_MAX
#undef NAME_MAX

#include "core/filer.h"
#include "core/terminal/terminal.h"

#define FILER_TASK_NAME "filer"
#define ACTION_TASK_NAME "action"

#define APP_MANIFEST_NAME "main.manifest"
#define APP_RUBY_MAIN_NAME "main.mrb"
#define APP_PYTHON_MAIN_NAME "main.mpy"
#define APP_README_NAME "README.md"
#define APP_IMAGE_NAME "image.h"
#define APP_IMAGE_HOLD_MILLISECONDS 3000

#define ETC_README_PATH_FORMAT "/etc/readme/%s.md"
#define SD_ERROR_RETRY_NAME "Retry SD mount"

#define COPY_BUFFER_BYTE_SIZE 512
#define ENTRY_NAMES_INITIAL_BYTE_CAPACITY 256
#define MANIFEST_LINE_BYTE_CAPACITY 256

typedef struct {
  int action;
  char target_path[CURRENT_DIRECTORY_MAX];
} ActionTaskRequest;

static ActionTaskRequest s_action_task_request;

static void run_filer_task(void *task_parameter);

// -----------------------------------------------------------------------------
// Paths
// -----------------------------------------------------------------------------

static void
join_path(
  const char *directory_path,
  const char *name,
  char *joined_path,
  int joined_path_size
) {

  if (strcmp(directory_path, "/") == 0)
    snprintf(joined_path, joined_path_size, "/%s", name);
  else
    snprintf(joined_path, joined_path_size, "%s/%s", directory_path, name);
}

static int
find_last_slash_index(const char *path) {
  int last_slash_index = -1;

  for (int i = 0; path[i]; i++) {
    if (path[i] == '/')
      last_slash_index = i;
  }

  return last_slash_index;
}

static const char *
base_name(const char *path) {
  int last_slash_index = find_last_slash_index(path);

  return last_slash_index < 0 ? path : path + last_slash_index + 1;
}

static void
dir_name(const char *path, char *directory_path, int directory_path_size) {
  int last_slash_index = find_last_slash_index(path);

  if (last_slash_index <= 0) {
    snprintf(directory_path, directory_path_size, "/");
    return;
  }

  snprintf(
    directory_path,
    directory_path_size,
    "%.*s",
    last_slash_index,
    path
  );
}

static void
strip_extension(const char *name, char *stripped_name, int stripped_name_size) {
  int kept_byte_count = (int)strlen(name);

  if (has_path_suffix(name, ".mrb") || has_path_suffix(name, ".mpy"))
    kept_byte_count -= 4;
  else if (has_path_suffix(name, ".rb") || has_path_suffix(name, ".py"))
    kept_byte_count -= 3;

  snprintf(stripped_name, stripped_name_size, "%.*s", kept_byte_count, name);
}

static void
build_bytecode_name(
  const char *name,
  const char *source_suffix,
  const char *bytecode_suffix,
  char *bytecode_name,
  int bytecode_name_size
) {

  if (!has_path_suffix(name, source_suffix)) {
    snprintf(bytecode_name, bytecode_name_size, "%s", name);
    return;
  }

  char stripped_name[CURRENT_DIRECTORY_MAX];

  strip_extension(name, stripped_name, sizeof stripped_name);

  snprintf(
    bytecode_name,
    bytecode_name_size,
    "%s%s",
    stripped_name,
    bytecode_suffix
  );
}

static int
path_exists(const char *path) {
  struct stat stat_buffer;

  return stat_data_path(path, &stat_buffer) == 0;
}

static int
path_is_directory(const char *path) {
  struct stat stat_buffer;

  return stat_data_path(path, &stat_buffer) == 0 &&
    S_ISDIR(stat_buffer.st_mode);
}

static void
format_runtime_error_message(
  char *message,
  int message_size,
  const char *exception_message
) {

  snprintf(message, message_size, "RuntimeError: %s", exception_message);
}

static int
resolve_data_path_or_format_error(
  const char *path,
  char *resolved_path,
  char *message,
  int message_size
) {

  if (area512_resolve_data_path(path, resolved_path, AREA512_PATH_MAX) != 0) {
    format_runtime_error_message(message, message_size, "invalid path");
    return -1;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Entry list
// -----------------------------------------------------------------------------

static char *
read_entry_names(const char *directory_path, int *entry_name_count) {
  char resolved_path[AREA512_PATH_MAX];

  if (
    area512_resolve_data_path(
      directory_path,
      resolved_path,
      sizeof resolved_path
    ) != 0
  ) {

    errno = EINVAL;
    return NULL;
  }

  DIR *directory = opendir(resolved_path);

  if (!directory)
    return NULL;

  size_t entry_names_byte_count = 0;
  size_t entry_names_byte_capacity = ENTRY_NAMES_INITIAL_BYTE_CAPACITY;
  char *entry_names = (char *)malloc(entry_names_byte_capacity);

  if (!entry_names) {
    closedir(directory);
    errno = ENOMEM;

    return NULL;
  }

  *entry_name_count = 0;

  struct dirent *directory_entry;

  while ((directory_entry = readdir(directory)) != NULL) {
    if (
      strcmp(directory_entry->d_name, ".") == 0 ||
      strcmp(directory_entry->d_name, "..") == 0
    )
      continue;

    size_t entry_name_byte_length = strlen(directory_entry->d_name);

    if (
      entry_names_byte_count + entry_name_byte_length + 1 >
      entry_names_byte_capacity
    ) {

      while (
        entry_names_byte_count + entry_name_byte_length + 1 >
        entry_names_byte_capacity
      )
        entry_names_byte_capacity *= 2;

      char *grown_entry_names =
        (char *)realloc(entry_names, entry_names_byte_capacity);

      if (!grown_entry_names) {
        free(entry_names);
        closedir(directory);
        errno = ENOMEM;

        return NULL;
      }

      entry_names = grown_entry_names;
    }

    memcpy(
      entry_names + entry_names_byte_count,
      directory_entry->d_name,
      entry_name_byte_length + 1
    );

    entry_names_byte_count += entry_name_byte_length + 1;
    (*entry_name_count)++;
  }

  closedir(directory);

  return entry_names;
}

static void
build_entries(Filer *filer) {
  FileEntry entry;

  filer->count = 0;

  if (strcmp(filer->current_directory, "/") != 0) {
    snprintf(entry.name, sizeof entry.name, "..");
    entry.type = ENTRY_TYPE_UP;
    add_entry_in_order(filer->entries, &filer->count, MAX_ENTRIES, &entry);
  }

  int entry_name_count = 0;
  char *entry_names =
    read_entry_names(filer->current_directory, &entry_name_count);

  if (!entry_names)
    return;

  const char *entry_name = entry_names;

  for (int i = 0; i < entry_name_count; i++) {
    char entry_path[CURRENT_DIRECTORY_MAX];

    join_path(
      filer->current_directory,
      entry_name,
      entry_path,
      sizeof entry_path
    );

    snprintf(entry.name, sizeof entry.name, "%s", entry_name);

    entry.type =
      path_is_directory(entry_path) ? ENTRY_TYPE_DIR : ENTRY_TYPE_FILE;

    add_entry_in_order(filer->entries, &filer->count, MAX_ENTRIES, &entry);

    entry_name += strlen(entry_name) + 1;
  }

  free(entry_names);
}

// -----------------------------------------------------------------------------
// README and app image
// -----------------------------------------------------------------------------

static void
show_readme(const char *readme_path) {
#ifdef AREA512_INTERNAL_DISPLAY
  show_internal_markdown(readme_path);
#else
  (void)readme_path;
#endif
}

static void
hide_readme(void) {
#ifdef AREA512_INTERNAL_DISPLAY
  hide_internal_markdown();
#endif
}

static void
show_app_readme(const char *app_directory_path) {
  char readme_path[CURRENT_DIRECTORY_MAX];

  join_path(
    app_directory_path,
    APP_README_NAME,
    readme_path,
    sizeof readme_path
  );

  show_readme(readme_path);
}

static void
show_etc_readme(const char *readme_name) {
  char readme_path[CURRENT_DIRECTORY_MAX];

  snprintf(
    readme_path,
    sizeof readme_path,
    ETC_README_PATH_FORMAT,
    readme_name
  );

  show_readme(readme_path);
}

static void
show_app_image(const char *app_directory_path) {
  char image_path[CURRENT_DIRECTORY_MAX];

  join_path(app_directory_path, APP_IMAGE_NAME, image_path, sizeof image_path);

  if (path_exists(image_path))
    area512_gfx_show_header_image(image_path, APP_IMAGE_HOLD_MILLISECONDS);
}

static int
is_python_manifest(const char *manifest_path) {
  char resolved_path[AREA512_PATH_MAX];

  if (
    area512_resolve_data_path(
      manifest_path,
      resolved_path,
      sizeof resolved_path
    ) != 0
  )
    return 0;

  FILE *manifest_file = fopen(resolved_path, "r");

  if (!manifest_file)
    return 0;

  char manifest_line[MANIFEST_LINE_BYTE_CAPACITY];
  int is_python = 0;

  while (fgets(manifest_line, sizeof manifest_line, manifest_file)) {
    char *manifest_cursor = manifest_line;
    char *first_entry_name =
      area512_split_next_manifest_entry(&manifest_cursor);

    if (first_entry_name) {
      is_python = has_path_suffix(first_entry_name, ".mpy");
      break;
    }
  }

  fclose(manifest_file);

  return is_python;
}

// -----------------------------------------------------------------------------
// Compile
// -----------------------------------------------------------------------------

static void
record_arena_used_byte_size_of_gc_heap(void) {
  area512_record_arena_used_byte_size(
    (int)area512_micropython_take_recorded_gc_heap_used_byte_size()
  );
}

static int
compile_source(
  const char *directory_path,
  const char *name,
  char *error_message,
  int error_message_size
) {

  char source_path[CURRENT_DIRECTORY_MAX];
  char bytecode_name[CURRENT_DIRECTORY_MAX];
  char bytecode_path[CURRENT_DIRECTORY_MAX];

  join_path(directory_path, name, source_path, sizeof source_path);

  if (has_path_suffix(name, ".py")) {
    build_bytecode_name(
      name,
      ".py",
      ".mpy",
      bytecode_name,
      sizeof bytecode_name
    );

    join_path(
      directory_path,
      bytecode_name,
      bytecode_path,
      sizeof bytecode_path
    );

    area512_console_reset();

    int compile_result =
      area512_micropython_compile_python_source_file(
        source_path,
        bytecode_path,
        g_area512_arena,
        AREA512_ARENA_BYTE_SIZE
      );

    record_arena_used_byte_size_of_gc_heap();

    area512_console_wait_key_if_output();

    if (compile_result != 0) {
      format_runtime_error_message(
        error_message,
        error_message_size,
        "Python compile failed"
      );

      return -1;
    }

    return 0;
  }

  build_bytecode_name(
    name,
    ".rb",
    ".mrb",
    bytecode_name,
    sizeof bytecode_name
  );

  join_path(
    directory_path,
    bytecode_name,
    bytecode_path,
    sizeof bytecode_path
  );

  const char *exception_message = NULL;

  area512_open_ruby_vm();

  int compile_result =
    area512_compile_ruby_source_file(
      source_path,
      bytecode_path,
      &exception_message
    );

  if (compile_result != 0)
    format_runtime_error_message(
      error_message,
      error_message_size,
      exception_message
    );

  area512_close_ruby_vm();

  return compile_result;
}

static void
compile_entry(
  const char *directory_path,
  const char *name,
  char *message,
  int message_size
) {

  if (compile_source(directory_path, name, message, message_size) != 0)
    return;

  snprintf(message, message_size, "Compiled %s", name);
}

static void
compile_all(const char *directory_path, char *message, int message_size) {
  int entry_name_count = 0;
  char *entry_names = read_entry_names(directory_path, &entry_name_count);

  if (!entry_names) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return;
  }

  const char *entry_name = entry_names;
  int compiled_count = 0;

  for (int i = 0; i < entry_name_count; i++) {
    if (
      has_path_suffix(entry_name, ".rb") ||
      has_path_suffix(entry_name, ".py")
    ) {

      if (
        compile_source(
          directory_path,
          entry_name,
          message,
          message_size
        ) != 0
      ) {

        free(entry_names);
        return;
      }

      compiled_count++;
    }

    entry_name += strlen(entry_name) + 1;
  }

  free(entry_names);

  if (compiled_count == 0) {
    snprintf(message, message_size, "No source here");
    return;
  }

  snprintf(message, message_size, "Compiled %d file(s)", compiled_count);
}

// -----------------------------------------------------------------------------
// Run
// -----------------------------------------------------------------------------

static SandboxState *
start_ruby_app(const char *label) {
  area512_console_reset();
  area512_open_ruby_vm();
  area512_run_environment_task();

  return area512_open_sandbox(label);
}

static void
finish_ruby_app(
  const char *label,
  int run_result,
  const char *exception_message,
  const char *exception_class_name,
  char *message,
  int message_size
) {

  switch (run_result) {
  case AREA512_SANDBOX_RUN_RAISED:
  case AREA512_SANDBOX_RUN_LOAD_FAILED:
    if (exception_class_name)
      mrbc_printf(
        "Error: %s: %s (%s)\n",
        label,
        exception_message,
        exception_class_name
      );
    else
      mrbc_printf("Error: %s: %s\n", label, exception_message);

    snprintf(message, message_size, "Error: %s", label);
    break;

  default:
    snprintf(message, message_size, "Returned: %s", label);
    break;
  }

  area512_close_ruby_vm();
  area512_console_wait_key_if_output();
}

static void
run_mrb(
  const char *mrb_path,
  const char *label,
  char *message,
  int message_size
) {

  SandboxState *sandbox_state = start_ruby_app(label);
  char exception_message_buffer[MESSAGE_MAX] =
    "failed to compile suspend_vm_code";
  const char *exception_class_name = "RuntimeError";
  int run_result = AREA512_SANDBOX_RUN_LOAD_FAILED;

  if (sandbox_state)
    run_result =
      area512_run_sandbox_mrb_file(
        sandbox_state,
        mrb_path,
        exception_message_buffer,
        sizeof exception_message_buffer,
        &exception_class_name
      );

  finish_ruby_app(
    label,
    run_result,
    exception_message_buffer,
    run_result == AREA512_SANDBOX_RUN_RAISED ? NULL : exception_class_name,
    message,
    message_size
  );
}

static void
run_manifest(
  const char *directory_path,
  const char *manifest_path,
  const char *label,
  char *message,
  int message_size
) {

  SandboxState *sandbox_state = start_ruby_app(label);
  char exception_message_buffer[MESSAGE_MAX] =
    "failed to compile suspend_vm_code";
  const char *exception_class_name = "RuntimeError";
  int run_result = AREA512_SANDBOX_RUN_LOAD_FAILED;

  if (sandbox_state)
    run_result =
      area512_run_sandbox_manifest(
        sandbox_state,
        directory_path,
        manifest_path,
        exception_message_buffer,
        sizeof exception_message_buffer,
        &exception_class_name
      );

  finish_ruby_app(
    label,
    run_result,
    exception_message_buffer,
    exception_class_name,
    message,
    message_size
  );
}

static void
run_mpy(
  const char *mpy_path,
  const char *label,
  char *message,
  int message_size
) {

  area512_console_reset();

  int run_result =
    area512_micropython_run_python_bytecode_file(
      mpy_path,
      g_area512_arena,
      AREA512_ARENA_BYTE_SIZE
    );

  record_arena_used_byte_size_of_gc_heap();

  if (run_result != 0)
    snprintf(message, message_size, "Error: %s", label);
  else
    snprintf(message, message_size, "Returned: %s", label);

  area512_console_wait_key_if_output();
}

static void
run_python_manifest(
  const char *directory_path,
  const char *manifest_path,
  const char *label,
  char *message,
  int message_size
) {

  area512_console_reset();

  int run_result =
    area512_micropython_run_python_manifest(
      directory_path,
      manifest_path,
      g_area512_arena,
      AREA512_ARENA_BYTE_SIZE
    );

  record_arena_used_byte_size_of_gc_heap();

  if (run_result != 0)
    snprintf(message, message_size, "Error: %s", label);
  else
    snprintf(message, message_size, "Returned: %s", label);

  area512_console_wait_key_if_output();
}

static void
run_ruby(
  const char *directory_path,
  const char *name,
  char *message,
  int message_size
) {

  if (
    has_path_suffix(name, ".rb") &&
    compile_source(directory_path, name, message, message_size) != 0
  )
    return;

  char bytecode_name[CURRENT_DIRECTORY_MAX];
  char mrb_path[CURRENT_DIRECTORY_MAX];
  char label[CURRENT_DIRECTORY_MAX];

  build_bytecode_name(
    name,
    ".rb",
    ".mrb",
    bytecode_name,
    sizeof bytecode_name
  );

  join_path(directory_path, bytecode_name, mrb_path, sizeof mrb_path);
  strip_extension(base_name(mrb_path), label, sizeof label);
  run_mrb(mrb_path, label, message, message_size);
}

static void
run_python(
  const char *directory_path,
  const char *name,
  char *message,
  int message_size
) {

  if (
    has_path_suffix(name, ".py") &&
    compile_source(directory_path, name, message, message_size) != 0
  )
    return;

  char bytecode_name[CURRENT_DIRECTORY_MAX];
  char mpy_path[CURRENT_DIRECTORY_MAX];
  char label[CURRENT_DIRECTORY_MAX];

  build_bytecode_name(
    name,
    ".py",
    ".mpy",
    bytecode_name,
    sizeof bytecode_name
  );

  join_path(directory_path, bytecode_name, mpy_path, sizeof mpy_path);
  strip_extension(base_name(mpy_path), label, sizeof label);
  run_mpy(mpy_path, label, message, message_size);
}

static void
run_dir(
  const char *directory_path,
  const char *name,
  char *message,
  int message_size
) {

  char app_directory_path[CURRENT_DIRECTORY_MAX];
  char manifest_path[CURRENT_DIRECTORY_MAX];
  char ruby_main_path[CURRENT_DIRECTORY_MAX];
  char python_main_path[CURRENT_DIRECTORY_MAX];

  join_path(
    directory_path,
    name,
    app_directory_path,
    sizeof app_directory_path
  );

  join_path(
    app_directory_path,
    APP_MANIFEST_NAME,
    manifest_path,
    sizeof manifest_path
  );

  join_path(
    app_directory_path,
    APP_RUBY_MAIN_NAME,
    ruby_main_path,
    sizeof ruby_main_path
  );

  join_path(
    app_directory_path,
    APP_PYTHON_MAIN_NAME,
    python_main_path,
    sizeof python_main_path
  );

  if (path_exists(manifest_path)) {
    show_app_readme(app_directory_path);
    show_app_image(app_directory_path);

    if (is_python_manifest(manifest_path))
      run_python_manifest(
        app_directory_path,
        manifest_path,
        name,
        message,
        message_size
      );
    else
      run_manifest(
        app_directory_path,
        manifest_path,
        name,
        message,
        message_size
      );

    hide_readme();

  } else if (path_exists(ruby_main_path)) {
    show_app_readme(app_directory_path);
    show_app_image(app_directory_path);
    run_mrb(ruby_main_path, name, message, message_size);
    hide_readme();

  } else if (path_exists(python_main_path)) {
    show_app_readme(app_directory_path);
    show_app_image(app_directory_path);
    run_mpy(python_main_path, name, message, message_size);
    hide_readme();

  } else {
    snprintf(
      message,
      message_size,
      "No main.manifest, main.mrb or main.mpy in %s",
      name
    );
  }
}

static void
run_irb(void) {
  area512_open_ruby_vm();
  area512_run_environment_task();
  area512_run_irb_task();
  area512_close_ruby_vm();
}

static void
run_python_repl(void) {
  area512_run_python_repl(g_area512_arena, AREA512_ARENA_BYTE_SIZE);

  record_arena_used_byte_size_of_gc_heap();
}

// -----------------------------------------------------------------------------
// Editors and viewers
// -----------------------------------------------------------------------------

static void
edit_entry(const char *path, char *message, int message_size) {
  const char *exception_message = NULL;

  show_etc_readme("vim");
  area512_open_ruby_vm();

  if (area512_edit_file(path, &exception_message) != 0)
    format_runtime_error_message(message, message_size, exception_message);
  else
    snprintf(message, message_size, "Returned from vim");

  area512_close_ruby_vm();
  hide_readme();
}

static void
view_markdown(const char *path, char *message, int message_size) {
  const char *exception_message = NULL;

  show_etc_readme("markdown");
  area512_open_ruby_vm();

  if (area512_view_markdown_file(path, &exception_message) != 0)
    format_runtime_error_message(message, message_size, exception_message);
  else
    snprintf(message, message_size, "Returned from viewer");

  area512_close_ruby_vm();
  hide_readme();
}

static void
edit_dot(const char *path, char *message, int message_size) {
  const char *exception_message = NULL;

  show_etc_readme("dot");

  if (area512_edit_dot_image_file(path, &exception_message) != 0)
    format_runtime_error_message(message, message_size, exception_message);
  else
    snprintf(message, message_size, "Returned from dot");

  hide_readme();
}

// -----------------------------------------------------------------------------
// Action task
// -----------------------------------------------------------------------------

static void
run_action_task(void *task_parameter) {
  Filer *filer = (Filer *)task_parameter;
  char target_directory_path[CURRENT_DIRECTORY_MAX];
  const char *target_name = base_name(s_action_task_request.target_path);

  dir_name(
    s_action_task_request.target_path,
    target_directory_path,
    sizeof target_directory_path
  );

  switch (s_action_task_request.action) {
  case ACTION_IRB:
    run_irb();
    break;

  case ACTION_PYTHON_REPL:
    run_python_repl();
    break;

  case ACTION_RUN_RUBY:
    run_ruby(target_directory_path, target_name, filer->message, MESSAGE_MAX);
    break;

  case ACTION_COMPILE:
    compile_entry(
      target_directory_path,
      target_name,
      filer->message,
      MESSAGE_MAX
    );

    break;

  case ACTION_COMPILE_ALL:
    compile_all(s_action_task_request.target_path, filer->message, MESSAGE_MAX);
    break;

  case ACTION_RUN_DIR:
    run_dir(target_directory_path, target_name, filer->message, MESSAGE_MAX);
    break;

  case ACTION_RUN_PYTHON:
    run_python(
      target_directory_path,
      target_name,
      filer->message,
      MESSAGE_MAX
    );

    break;

  case ACTION_EDIT:
    edit_entry(s_action_task_request.target_path, filer->message, MESSAGE_MAX);
    break;

  case ACTION_VIEW_MARKDOWN:
    view_markdown(
      s_action_task_request.target_path,
      filer->message,
      MESSAGE_MAX
    );

    break;
  }

  area512_switch_shared_stack_task(FILER_TASK_NAME, run_filer_task, filer);
}

// -----------------------------------------------------------------------------
// Create / delete / move / copy
// -----------------------------------------------------------------------------

static void
create_file(
  const char *current_directory,
  const char *name,
  char *message,
  int message_size
) {

  if (name[0] == '\0') {
    snprintf(message, message_size, "Empty name");
    return;
  }

  char entry_path[CURRENT_DIRECTORY_MAX];
  char resolved_path[AREA512_PATH_MAX];

  join_path(current_directory, name, entry_path, sizeof entry_path);

  if (path_exists(entry_path)) {
    snprintf(message, message_size, "Already exists");
    return;
  }

  if (
    resolve_data_path_or_format_error(
      entry_path,
      resolved_path,
      message,
      message_size
    ) != 0
  )
    return;

  FILE *created_file = fopen(resolved_path, "w");

  if (!created_file) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return;
  }

  fclose(created_file);

  snprintf(message, message_size, "Created %s", name);
}

static void
create_dir(
  const char *current_directory,
  const char *name,
  char *message,
  int message_size
) {

  if (name[0] == '\0') {
    snprintf(message, message_size, "Empty name");
    return;
  }

  char entry_path[CURRENT_DIRECTORY_MAX];
  char resolved_path[AREA512_PATH_MAX];

  join_path(current_directory, name, entry_path, sizeof entry_path);

  if (path_exists(entry_path)) {
    snprintf(message, message_size, "Already exists");
    return;
  }

  if (
    resolve_data_path_or_format_error(
      entry_path,
      resolved_path,
      message,
      message_size
    ) != 0
  )
    return;

  if (mkdir(resolved_path, 0777) != 0) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return;
  }

  snprintf(message, message_size, "Created %s/", name);
}

static int
remove_tree(const char *path, char *message, int message_size) {
  char resolved_path[AREA512_PATH_MAX];

  if (!path_is_directory(path)) {
    if (
      resolve_data_path_or_format_error(
        path,
        resolved_path,
        message,
        message_size
      ) != 0
    )
      return -1;

    if (unlink(resolved_path) != 0) {
      format_runtime_error_message(message, message_size, strerror(errno));
      return -1;
    }

    return 0;
  }

  int entry_name_count = 0;
  char *entry_names = read_entry_names(path, &entry_name_count);

  if (!entry_names) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return -1;
  }

  const char *entry_name = entry_names;

  for (int i = 0; i < entry_name_count; i++) {
    char entry_path[CURRENT_DIRECTORY_MAX];

    join_path(path, entry_name, entry_path, sizeof entry_path);

    if (remove_tree(entry_path, message, message_size) != 0) {
      free(entry_names);
      return -1;
    }

    entry_name += strlen(entry_name) + 1;
  }

  free(entry_names);

  if (
    resolve_data_path_or_format_error(
      path,
      resolved_path,
      message,
      message_size
    ) != 0
  )
    return -1;

  if (rmdir(resolved_path) != 0) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return -1;
  }

  return 0;
}

static void
delete_entry(const char *path, char *message, int message_size) {
  if (remove_tree(path, message, message_size) != 0)
    return;

  snprintf(message, message_size, "Deleted %s", base_name(path));
}

static int
resolve_destination_path(
  const char *current_directory,
  const char *source_path,
  int source_is_directory,
  const char *destination_input,
  const char *operation_verb,
  char *destination_path,
  int destination_path_size,
  char *message,
  int message_size
) {

  if (destination_input[0] == '\0') {
    snprintf(message, message_size, "Empty path");
    return -1;
  }

  char normalized_input_path[CURRENT_DIRECTORY_MAX];
  int input_is_directory = 0;

  int target_result =
    resolve_target_path(
      current_directory,
      destination_input,
      normalized_input_path,
      sizeof normalized_input_path,
      &input_is_directory
    );

  if (target_result == TARGET_INVALID) {
    snprintf(message, message_size, "Bad path");
    return -1;
  }

  if (target_result == TARGET_RESOLVED && input_is_directory)
    join_path(
      normalized_input_path,
      base_name(source_path),
      destination_path,
      destination_path_size
    );
  else
    snprintf(
      destination_path,
      destination_path_size,
      "%s",
      normalized_input_path
    );

  char destination_directory_path[CURRENT_DIRECTORY_MAX];

  dir_name(
    destination_path,
    destination_directory_path,
    sizeof destination_directory_path
  );

  if (!path_is_directory(destination_directory_path)) {
    snprintf(message, message_size, "No such directory");
    return -1;
  }

  if (strcmp(destination_path, source_path) == 0) {
    snprintf(message, message_size, "Same path");
    return -1;
  }

  if (
    source_is_directory &&
    strncmp(destination_path, source_path, strlen(source_path)) == 0 &&
    destination_path[strlen(source_path)] == '/'
  ) {

    snprintf(message, message_size, "Can't %s into itself", operation_verb);
    return -1;
  }

  if (path_exists(destination_path)) {
    snprintf(
      message,
      message_size,
      "Already exists: %s",
      base_name(destination_path)
    );

    return -1;
  }

  return 0;
}

static void
move_entry(
  const char *current_directory,
  const char *source_path,
  const char *destination_input,
  char *message,
  int message_size
) {

  char destination_path[CURRENT_DIRECTORY_MAX];

  if (
    resolve_destination_path(
      current_directory,
      source_path,
      path_is_directory(source_path),
      destination_input,
      "move",
      destination_path,
      sizeof destination_path,
      message,
      message_size
    ) != 0
  )
    return;

  char resolved_source_path[AREA512_PATH_MAX];
  char resolved_destination_path[AREA512_PATH_MAX];

  if (
    resolve_data_path_or_format_error(
      source_path,
      resolved_source_path,
      message,
      message_size
    ) != 0 ||
    resolve_data_path_or_format_error(
      destination_path,
      resolved_destination_path,
      message,
      message_size
    ) != 0
  )
    return;

  if (rename(resolved_source_path, resolved_destination_path) != 0) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return;
  }

  snprintf(
    message,
    message_size,
    "Moved %s -> %s",
    base_name(source_path),
    destination_path
  );
}

static int
copy_file(
  const char *source_path,
  const char *destination_path,
  char *message,
  int message_size
) {

  char resolved_source_path[AREA512_PATH_MAX];
  char resolved_destination_path[AREA512_PATH_MAX];

  if (
    resolve_data_path_or_format_error(
      source_path,
      resolved_source_path,
      message,
      message_size
    ) != 0 ||
    resolve_data_path_or_format_error(
      destination_path,
      resolved_destination_path,
      message,
      message_size
    ) != 0
  )
    return -1;

  FILE *source_file = fopen(resolved_source_path, "rb");

  if (!source_file) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return -1;
  }

  FILE *destination_file = fopen(resolved_destination_path, "wb");

  if (!destination_file) {
    format_runtime_error_message(message, message_size, strerror(errno));
    fclose(source_file);

    return -1;
  }

  char copy_buffer[COPY_BUFFER_BYTE_SIZE];
  size_t read_byte_count;
  int copy_result = 0;

  while (
    (read_byte_count = fread(copy_buffer, 1, sizeof copy_buffer, source_file)) >
    0
  ) {

    if (
      fwrite(copy_buffer, 1, read_byte_count, destination_file) !=
      read_byte_count
    ) {

      format_runtime_error_message(message, message_size, strerror(errno));
      copy_result = -1;

      break;
    }
  }

  if (copy_result == 0 && ferror(source_file)) {
    format_runtime_error_message(message, message_size, strerror(errno));
    copy_result = -1;
  }

  fclose(source_file);

  if (fclose(destination_file) != 0 && copy_result == 0) {
    format_runtime_error_message(message, message_size, strerror(errno));
    copy_result = -1;
  }

  return copy_result;
}

static int
copy_tree(
  const char *source_path,
  const char *destination_path,
  char *message,
  int message_size
) {

  if (!path_is_directory(source_path))
    return copy_file(source_path, destination_path, message, message_size);

  char resolved_path[AREA512_PATH_MAX];

  if (
    resolve_data_path_or_format_error(
      destination_path,
      resolved_path,
      message,
      message_size
    ) != 0
  )
    return -1;

  if (mkdir(resolved_path, 0777) != 0) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return -1;
  }

  int entry_name_count = 0;
  char *entry_names = read_entry_names(source_path, &entry_name_count);

  if (!entry_names) {
    format_runtime_error_message(message, message_size, strerror(errno));
    return -1;
  }

  const char *entry_name = entry_names;

  for (int i = 0; i < entry_name_count; i++) {
    char source_entry_path[CURRENT_DIRECTORY_MAX];
    char destination_entry_path[CURRENT_DIRECTORY_MAX];

    join_path(
      source_path,
      entry_name,
      source_entry_path,
      sizeof source_entry_path
    );

    join_path(
      destination_path,
      entry_name,
      destination_entry_path,
      sizeof destination_entry_path
    );

    if (
      copy_tree(
        source_entry_path,
        destination_entry_path,
        message,
        message_size
      ) != 0
    ) {

      free(entry_names);
      return -1;
    }

    entry_name += strlen(entry_name) + 1;
  }

  free(entry_names);

  return 0;
}

static void
copy_entry(
  const char *current_directory,
  const char *source_path,
  const char *destination_input,
  char *message,
  int message_size
) {

  char destination_path[CURRENT_DIRECTORY_MAX];

  if (
    resolve_destination_path(
      current_directory,
      source_path,
      path_is_directory(source_path),
      destination_input,
      "copy",
      destination_path,
      sizeof destination_path,
      message,
      message_size
    ) != 0
  )
    return;

  if (copy_tree(source_path, destination_path, message, message_size) != 0)
    return;

  snprintf(
    message,
    message_size,
    "Copied %s -> %s",
    base_name(source_path),
    destination_path
  );
}

static void
change_directory(Filer *filer) {
  char normalized_input_path[CURRENT_DIRECTORY_MAX];
  int input_is_directory = 0;

  int target_result =
    resolve_target_path(
      filer->current_directory,
      filer->input,
      normalized_input_path,
      sizeof normalized_input_path,
      &input_is_directory
    );

  if (target_result == TARGET_INVALID) {
    snprintf(filer->message, MESSAGE_MAX, "Bad path");
    return;
  }

  if (target_result != TARGET_RESOLVED || !input_is_directory) {
    snprintf(filer->message, MESSAGE_MAX, "No such directory");
    return;
  }

  snprintf(
    filer->current_directory,
    CURRENT_DIRECTORY_MAX,
    "%s",
    normalized_input_path
  );

  filer->index = 0;
}

// -----------------------------------------------------------------------------
// Filer task
// -----------------------------------------------------------------------------

static void
run_sd_error_screen(Filer *filer) {
  FileEntry entry;

  snprintf(filer->current_directory, CURRENT_DIRECTORY_MAX, "/");
  filer->count = 0;

  snprintf(entry.name, sizeof entry.name, SD_ERROR_RETRY_NAME);
  entry.type = ENTRY_TYPE_UP;
  add_entry_in_order(filer->entries, &filer->count, MAX_ENTRIES, &entry);

  snprintf(
    filer->message,
    MESSAGE_MAX,
    "No SD card: Enter = retry, r = reboot"
  );

  for (;;) {
    if (read_filer_action(filer) == ACTION_REBOOT)
      esp_restart();

    if (area512_sd_mount("/sdcard") == 0)
      return;

    snprintf(
      filer->message,
      MESSAGE_MAX,
      "SD card not found: Enter = retry, r = reboot"
    );
  }
}

static int
mount_sd_and_restore_seed(Filer *filer) {
  if (area512_sd_mount("/sdcard") != 0)
    run_sd_error_screen(filer);

  if (area512_seed_restore() != 0) {
    mrbc_printf("seed restore failed (RuntimeError)\n");
    return -1;
  }

  area512_theme_load();

  return 0;
}

static void
run_filer_task(void *task_parameter) {
  Filer *filer = (Filer *)task_parameter;

  if (s_action_task_request.action == ACTION_NONE) {
    if (mount_sd_and_restore_seed(filer) != 0)
      vTaskDelete(NULL);

    snprintf(filer->current_directory, CURRENT_DIRECTORY_MAX, "/home");
  }

  s_action_task_request.action = ACTION_NONE;
  build_entries(filer);

  int is_readme_shown = 0;

  for (;;) {
    if (!is_readme_shown) {
      show_etc_readme("filer");
      is_readme_shown = 1;
    }

    int action = read_filer_action(filer);

    if (
      action != ACTION_OPEN_DIR &&
      action != ACTION_UP &&
      action != ACTION_CHANGE_DIR
    ) {

      hide_readme();
      is_readme_shown = 0;
    }

    char target_path[CURRENT_DIRECTORY_MAX];
    FileEntry *selected_entry = fetch_selected_entry(filer);

    if (filer->action_target_path[0] == '\0')
      join_path(
        filer->current_directory,
        selected_entry ? selected_entry->name : "",
        target_path,
        sizeof target_path
      );
    else
      snprintf(
        target_path,
        sizeof target_path,
        "%s",
        filer->action_target_path
      );

    filer->message[0] = '\0';

    switch (action) {
    case ACTION_OPEN_DIR: {
      char opened_directory_path[CURRENT_DIRECTORY_MAX];

      join_path(
        filer->current_directory,
        selected_entry ? selected_entry->name : "",
        opened_directory_path,
        sizeof opened_directory_path
      );

      snprintf(
        filer->current_directory,
        CURRENT_DIRECTORY_MAX,
        "%s",
        opened_directory_path
      );

      filer->index = 0;
      break;
    }

    case ACTION_UP: {
      char parent_directory_path[CURRENT_DIRECTORY_MAX];

      dir_name(
        filer->current_directory,
        parent_directory_path,
        sizeof parent_directory_path
      );

      snprintf(
        filer->current_directory,
        CURRENT_DIRECTORY_MAX,
        "%s",
        parent_directory_path
      );

      filer->index = 0;
      break;
    }

    case ACTION_CHANGE_DIR:
      change_directory(filer);
      break;

    case ACTION_COMPILE_ALL:
      s_action_task_request.action = action;

      snprintf(
        s_action_task_request.target_path,
        CURRENT_DIRECTORY_MAX,
        "%s",
        filer->action_target_path[0] == '\0'
          ? filer->current_directory
          : target_path
      );

      area512_switch_shared_stack_task(
        ACTION_TASK_NAME,
        run_action_task,
        filer
      );

      break;

    case ACTION_IRB:
    case ACTION_PYTHON_REPL:
    case ACTION_RUN_RUBY:
    case ACTION_COMPILE:
    case ACTION_RUN_DIR:
    case ACTION_RUN_PYTHON:
    case ACTION_EDIT:
    case ACTION_VIEW_MARKDOWN:
      s_action_task_request.action = action;

      snprintf(
        s_action_task_request.target_path,
        CURRENT_DIRECTORY_MAX,
        "%s",
        target_path
      );

      area512_switch_shared_stack_task(
        ACTION_TASK_NAME,
        run_action_task,
        filer
      );

      break;

    case ACTION_EDIT_DOT:
      edit_dot(target_path, filer->message, MESSAGE_MAX);
      break;

    case ACTION_NEW_FILE:
      create_file(
        filer->current_directory,
        filer->input,
        filer->message,
        MESSAGE_MAX
      );

      break;

    case ACTION_NEW_DIR:
      create_dir(
        filer->current_directory,
        filer->input,
        filer->message,
        MESSAGE_MAX
      );

      break;

    case ACTION_DELETE:
      delete_entry(target_path, filer->message, MESSAGE_MAX);
      break;

    case ACTION_MOVE:
      move_entry(
        filer->current_directory,
        target_path,
        filer->input,
        filer->message,
        MESSAGE_MAX
      );

      break;

    case ACTION_COPY:
      copy_entry(
        filer->current_directory,
        target_path,
        filer->input,
        filer->message,
        MESSAGE_MAX
      );

      break;

    case ACTION_REBOOT:
      esp_restart();
      break;

    case ACTION_RECEIVE_FILES:
      area512_usb_receive_files();
      break;

    case ACTION_INSTALL_APP_IMAGE:
      area512_install_app_image(target_path, filer->message, MESSAGE_MAX);
      break;

    case ACTION_LAUNCH_INSTALLED_APP:
      area512_launch_installed_app(filer->message, MESSAGE_MAX);
      break;

    case ACTION_UNINSTALL_APP:
      area512_uninstall_app(filer->message, MESSAGE_MAX);
      break;
    }

    build_entries(filer);
  }
}

void
area512_start_filer(void) {
  Filer *filer = (Filer *)malloc(sizeof(Filer));

  if (filer == NULL) {
    mrbc_printf("failed to allocate Filer (RuntimeError)\n");
    return;
  }

  memset(filer, 0, sizeof(Filer));
  init_filer_state(filer);

  if (area512_default_ui_is_terminal())
    open_terminal_session(filer);

  area512_start_shared_stack_task(FILER_TASK_NAME, run_filer_task, filer);
}
