#if defined(PICORB_VM_MRUBYC)

#include "area512_micropython.h"

#include <stdlib.h>

#include <mrubyc.h>

static void
c_micropython_compile_python_source_file(
  mrbc_vm *virtual_machine,
  mrbc_value *ruby_method_arguments,
  int ruby_method_argument_count
) {

  if (ruby_method_argument_count < 2) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(ArgumentError),
      "wrong number of arguments (expected 2)"
    );

    return;
  }

  if (
    ruby_method_arguments[1].tt != MRBC_TT_STRING ||
    ruby_method_arguments[2].tt != MRBC_TT_STRING
  ) {

    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(TypeError),
      "expected String Python source, bytecode path"
    );

    return;
  }

  const char *python_source_path =
    (const char *)ruby_method_arguments[1].string->data;

  const char *python_bytecode_path =
    (const char *)ruby_method_arguments[2].string->data;

  size_t gc_heap_byte_size = 0;
  void *gc_heap = area512_micropython_allocate_gc_heap(&gc_heap_byte_size);

  int compile_result = -1;

  if (gc_heap) {
    compile_result =
      area512_micropython_compile_python_source_file(
        python_source_path,
        python_bytecode_path,
        gc_heap,
        gc_heap_byte_size
      );

    free(gc_heap);
  }

  if (compile_result != 0) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(RuntimeError),
      "Python compile failed"
    );

    return;
  }

  mrbc_decref(ruby_method_arguments);
  mrbc_set_nil(ruby_method_arguments);
}

static void
c_micropython_run_python_bytecode_file(
  mrbc_vm *virtual_machine,
  mrbc_value *ruby_method_arguments,
  int ruby_method_argument_count
) {

  if (ruby_method_argument_count < 1) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(ArgumentError),
      "wrong number of arguments (expected 1)"
    );

    return;
  }

  if (ruby_method_arguments[1].tt != MRBC_TT_STRING) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(TypeError),
      "expected String Python bytecode path"
    );

    return;
  }

  const char *python_bytecode_path =
    (const char *)ruby_method_arguments[1].string->data;

  size_t gc_heap_byte_size = 0;
  void *gc_heap = area512_micropython_allocate_gc_heap(&gc_heap_byte_size);

  int run_result = -1;

  if (gc_heap) {
    run_result =
      area512_micropython_run_python_bytecode_file(
        python_bytecode_path,
        gc_heap,
        gc_heap_byte_size
      );

    free(gc_heap);
  }

  if (run_result != 0) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(RuntimeError),
      "Python run failed"
    );

    return;
  }

  mrbc_decref(ruby_method_arguments);
  mrbc_set_nil(ruby_method_arguments);
}

static void
c_micropython_run_python_manifest(
  mrbc_vm *virtual_machine,
  mrbc_value *ruby_method_arguments,
  int ruby_method_argument_count
) {

  if (ruby_method_argument_count < 2) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(ArgumentError),
      "wrong number of arguments (expected 2)"
    );

    return;
  }

  if (
    ruby_method_arguments[1].tt != MRBC_TT_STRING ||
    ruby_method_arguments[2].tt != MRBC_TT_STRING
  ) {

    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(TypeError),
      "expected String directory, manifest path"
    );

    return;
  }

  const char *directory_path =
    (const char *)ruby_method_arguments[1].string->data;

  const char *manifest_path =
    (const char *)ruby_method_arguments[2].string->data;

  size_t gc_heap_byte_size = 0;
  void *gc_heap = area512_micropython_allocate_gc_heap(&gc_heap_byte_size);

  int run_result = -1;

  if (gc_heap) {
    run_result =
      area512_micropython_run_python_manifest(
        directory_path,
        manifest_path,
        gc_heap,
        gc_heap_byte_size
      );

    free(gc_heap);
  }

  if (run_result != 0) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(RuntimeError),
      "Python run failed"
    );

    return;
  }

  mrbc_decref(ruby_method_arguments);
  mrbc_set_nil(ruby_method_arguments);
}

void
mrbc_area512_micropython_init(mrbc_vm *virtual_machine) {
  mrbc_class *micro_python_class =
    mrbc_define_class(virtual_machine, "MicroPython", mrbc_class_object);

  mrbc_define_method(
    virtual_machine,
    micro_python_class,
    "compile_file",
    c_micropython_compile_python_source_file
  );

  mrbc_define_method(
    virtual_machine,
    micro_python_class,
    "run_bytecode_file",
    c_micropython_run_python_bytecode_file
  );

  mrbc_define_method(
    virtual_machine,
    micro_python_class,
    "run_manifest",
    c_micropython_run_python_manifest
  );
}

#endif
