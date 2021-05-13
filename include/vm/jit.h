#pragma once

#define ASMJIT_STATIC

#include <cstdint>
#include <asmjit/asmjit.h>

#include <vm/vm.h>

using namespace std;

namespace zero {

    typedef union {
        int64_t int_value;
        uint64_t uint_vaLue;
        string *str_value;
        double decimal_value;
    } z_op_t;

    // Signature of the generated function.
    typedef int (*z_jit_fnc)();

    // opcode handler
    typedef uint64_t (z_opcode_handler)(z_op_t, z_op_t, z_op_t);

    // It basically calls every function handler. only reduces dispatch overhead and that's all
    // compiles fast and un optimised code
    z_jit_fnc baseline_jit(Program* program, z_opcode_handler** handlers);

}