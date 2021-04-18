#pragma once

#include <common/program.h>

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

    typedef z_value_t (*z_native_fnc_t)(z_value_t*, unsigned int);

    void vm_run(Program *program);

}