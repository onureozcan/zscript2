#pragma once

#include <common/program.h>

#define STACK_MAX 1000

namespace zero {

    typedef struct {
        union {
            uint64_t uint_value;
            int64_t int_value;
            double double_value;
            char* string_value;
            void* ptr_value;

        };
    } z_value_t;

    void push(z_value_t value);

    z_value_t pop();

    // a native function manages stack manually. no calling convention yet
    typedef z_value_t (*z_native_fnc_t)(z_value_t *stack, unsigned int *sp_ptr);

    void vm_run(Program *program);

    z_value_t native_print(z_value_t *stack, unsigned int *sp_ptr);

}