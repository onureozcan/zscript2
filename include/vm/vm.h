#pragma once

#include <common/program.h>
#include <common/logger.h>

#define STACK_MAX 10000

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

    typedef struct {
        union {
            void *branch_addr;
            uint64_t opcode;
        };
        union {
            uint64_t op1;
            string *op1_string;
        };
        uint64_t op2;
        uint64_t destination;
    } vm_instruction_t;

    extern Logger vm_log;

    // for parameter passing, return address etc
    extern int64_t stack_pointer;
    extern z_value_t value_stack[STACK_MAX];

    // a native function manages stack manually. no calling convention yet
    typedef z_value_t (*z_native_fnc_t)();

    void vm_run_direct_threading(Program *program);

    void vm_run_context_threading(Program *program);
}