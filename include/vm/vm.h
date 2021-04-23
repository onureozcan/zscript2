#pragma once

#include <common/program.h>

#define STACK_MAX 1000

namespace zero {

    typedef struct {
        // because parent and callee are not always the same!.
        // the context, which creates the function reference, by MOV_FNC, is the parent of the function that was referred to!
        // let's say the function A contains function B and B requires some variable in A's context
        // in that case, B, has to have a reference to A's context, and this is how it is achieved
        // A creates a z_value of type function_ref, and this ref contains both the index and the A's reference to it
        // when some other function, say C, calls B, it has to use the value created by A, because there is no other way for it to access B
        // and this way, upon calling B, C pushes parent context, which is A to the stack
        void* parent_context_ptr;
        uint64_t instruction_index; // not the pointer but the index
    } z_fnc_ref_t;

    typedef struct {
        union {
            uint64_t uint_value;
            int64_t int_value;
            double double_value;
            string* string_value;
            void* ptr_value;
        };
    } z_value_t;

    void push(z_value_t value);

    z_value_t pop();

    // a native function manages stack manually. no calling convention yet
    typedef z_value_t (*z_native_fnc_t)();

    void vm_run(Program *program);

    z_value_t native_print();

}