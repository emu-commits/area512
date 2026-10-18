#pragma once

#include <stddef.h>

#include <mrubyc.h>

void define_python_repl_class(mrbc_vm *virtual_machine);
void area512_run_python_repl(void *gc_heap, size_t gc_heap_byte_size);
