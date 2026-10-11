#include <stdint.h>

#if defined(PICORB_VM_MRUBYC)

#include "area512_compile.h"
#include "area512_hal.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <mrc_codegen.h>
#include <mrc_presym.h>
#include <mruby_compiler.h>
#include <mrubyc.h>

static uint8_t *
read_whole_file(
  const char *path,
  size_t *size,
  const char **exception_message
) {

  char full_path[AREA512_PATH_MAX];

  if (area512_resolve_data_path(path, full_path, sizeof full_path) != 0) {
    *exception_message = "invalid path";
    return NULL;
  }

  FILE *file = fopen(full_path, "rb");

  if (!file) {
    *exception_message = strerror(errno);
    return NULL;
  }

  long file_size = -1;

  if (fseek(file, 0, SEEK_END) == 0) {
    file_size = ftell(file);
  }

  if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    *exception_message = strerror(errno);

    return NULL;
  }

  uint8_t *buffer = (uint8_t *)mrbc_raw_alloc((unsigned int)file_size + 1);
  if (!buffer) {
    fclose(file);
    *exception_message = "out of memory";

    return NULL;
  }

  size_t read_size = fread(buffer, 1, (size_t)file_size, file);

  fclose(file);

  if (read_size != (size_t)file_size) {
    mrbc_raw_free(buffer);
    *exception_message = "read failed";

    return NULL;
  }

  buffer[read_size] = '\0';
  *size = read_size;

  return buffer;
}

static bool
write_whole_file(
  const char *path,
  const uint8_t *data,
  size_t size,
  const char **exception_message
) {

  char full_path[AREA512_PATH_MAX];

  if (area512_resolve_data_path(path, full_path, sizeof full_path) != 0) {
    *exception_message = "invalid path";
    return false;
  }

  FILE *file = fopen(full_path, "wb");

  if (!file) {
    *exception_message = strerror(errno);
    return false;
  }

  size_t written = fwrite(data, 1, size, file);

  if (fclose(file) != 0 || written != size) {
    *exception_message = "write failed";
    return false;
  }

  return true;
}

int
area512_compile_ruby_source_file(
  const char *source_path,
  const char *bytecode_path,
  const char **exception_message
) {

  size_t source_length = 0;

  uint8_t *source =
    read_whole_file(source_path, &source_length, exception_message);

  if (!source)
    return -1;

  if (source_length >= 4 && memcmp(source, "RITE", 4) == 0) {
    mrbc_raw_free(source);
    *exception_message = "already compiled";

    return -1;
  }

  mrc_ccontext *context = mrc_ccontext_new(NULL);
  if (!context) {
    mrbc_raw_free(source);
    *exception_message = "out of memory";

    return -1;
  }

  mrc_ccontext_filename(context, source_path);

  context->filename_table =
    (mrc_filename_table *)mrbc_raw_alloc(sizeof(mrc_filename_table));

  context->filename_table[0].filename =
    context->filename ? context->filename : source_path;

  context->filename_table[0].start = 0;
  context->filename_table_length = 1;
  context->current_filename_index = 0;

  pm_options_t options;
  memset(&options, 0, sizeof(options));
  pm_string_constant_init(&options.encoding, "UTF-8", 5);

  pm_parser_init(context->p, source, source_length, &options);
  mrc_init_presym(&context->p->constant_pool);
  mrc_node *root = pm_parse(context->p);

  mrc_irep *irep = NULL;
  if (context->p->error_list.size == 0)
    irep = mrc_generate_code(context, root);

  uint8_t *binary = NULL;
  size_t binary_size = 0;
  int dump_result = -1;
  bool wrote = false;

  if (irep) {
    mrc_irep_remove_lv(context, irep);
    dump_result = mrc_dump_irep(context, irep, 0, &binary, &binary_size);

    if (dump_result == MRC_DUMP_OK)
      wrote = write_whole_file(
        bytecode_path,
        binary,
        binary_size,
        exception_message
      );
  }

  if (binary)
    mrbc_raw_free(binary);

  if (irep)
    mrc_irep_free(context, irep);

  pm_node_destroy(context->p, root);
  mrc_ccontext_free(context);
  pm_options_free(&options);
  mrbc_raw_free(source);

  if (!irep) {
    *exception_message = "compile failed";

    return -1;
  }

  if (dump_result != MRC_DUMP_OK) {
    *exception_message = "dump failed";

    return -1;
  }

  if (!wrote)
    return -1;

  return 0;
}

static void
c_area512_compile_file(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)argument_count;

  if (v[1].tt != MRBC_TT_STRING || v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(TypeError),
      "expected String source, destination"
    );

    return;
  }

  const char *source_path = (const char *)v[1].string->data;
  const char *destination_path = (const char *)v[2].string->data;
  const char *exception_message = NULL;

  if (
    area512_compile_ruby_source_file(
      source_path,
      destination_path,
      &exception_message
    ) != 0
  ) {

    mrbc_raise(virtual_machine, MRBC_CLASS(RuntimeError), exception_message);

    return;
  }

  SET_TRUE_RETURN();
}

void
mrbc_area512_compile_init(mrbc_vm *virtual_machine) {
  mrbc_class *class_Area512 =
    mrbc_define_class(virtual_machine, "Area512", mrbc_class_object);

  mrbc_define_method(
    virtual_machine,
    class_Area512,
    "compile_file",
    c_area512_compile_file
  );
}

#else // PICORB_VM_MRUBY: unused in this build; empty stubs

#include <mruby.h>

void
mrb_picoruby_area512_compile_gem_init(mrb_state *state) {
  (void)state;
}

void
mrb_picoruby_area512_compile_gem_final(mrb_state *state) {
  (void)state;
}

#endif
