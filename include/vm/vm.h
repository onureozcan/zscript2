#pragma once

#include <common/program.h>
#include <common/logger.h>

#define STACK_MAX 100000

#define PRIMITIVE_TYPE_INT 1
#define PRIMITIVE_TYPE_DOUBLE 2
#define PRIMITIVE_TYPE_BOOLEAN 3

namespace zero {

    typedef struct {
        union {
            uint64_t uint_value;
            int32_t int_value;
            struct {
                // TODO: endianness !!!!!
                uint32_t primitive_type;
                union {
                    // DO NOT USE THEM EXCEPT FOR CALCULATIONS
                    int32_t arithmetic_int_value;
                    float arithmetic_decimal_value;
                };
            };
            string *string_value;
            void *ptr_value;
        };
    } z_value_t;

    void push(z_value_t value);

    z_value_t pop();

    // a native function manages stack manually. no calling convention yet
    typedef z_value_t (*z_native_fnc_t)();

    void vm_run(Program *program);

    z_value_t native_print();

}