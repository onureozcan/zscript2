#include <vm/vm.h>
#include <vm/object_manager.h>
#include <vm/shared.h>

#include <common/util.h>

#include <cmath>

#include <vm/jit.h>

//#define VM_DEBUG_ACTIVE

#ifdef VM_DEBUG_ACTIVE
#define VM_DEBUG(ARGS) vm_log.debug ARGS
#else
#define VM_DEBUG(ARGS)
#endif

#define DESTINATION_PTR ((z_value_t*)(((uintptr_t)context_object) + dest.uint_vaLue))
#define OP1_PTR ((z_value_t*)(((uintptr_t)context_object) + op1.uint_vaLue))
#define OP2_PTR ((z_value_t*)(((uintptr_t)context_object) + op2.uint_vaLue))

#include "../../vm_shared_inline.cpp"

using namespace std;

namespace zero {

    register z_value_t *context_object asm ("r12");
    int64_t base_pointer;
    uint64_t call_depth;

    uint64_t z_handler_FN_ENTER_HEAP(z_op_t local_values_size, z_op_t op2, z_op_t dest) {
        auto parent_context = (z_value_t *) pop().ptr_value;
        context_object = alloc(local_values_size.uint_vaLue);
        init_call_context(context_object, parent_context);

        VM_DEBUG(("function enter heap, bp: %d, sp: %d", base_pointer, stack_pointer));
        push(uvalue(base_pointer));
        base_pointer = stack_pointer;
        call_depth++;
        return 0;
    }

    uint64_t z_handler_FN_ENTER_STACK(z_op_t local_values_size, z_op_t op2, z_op_t dest) {
        auto parent_context = (z_value_t *) pop().ptr_value;
        VM_DEBUG(("function enter stack, bp: %d, sp: %d", base_pointer, stack_pointer));
        push(uvalue(base_pointer));
        base_pointer = stack_pointer;
        call_depth++;
        context_object = &value_stack[stack_pointer];
        init_call_context(context_object, parent_context);

        stack_pointer += local_values_size.uint_vaLue;
        if (stack_pointer > STACK_MAX) {
            vm_log.error("could not allocate local stack frame, stack overflow!");
            exit(1);
        }

        VM_DEBUG(("stack allocated %d, sp: %d", local_values_size, stack_pointer));
        return 0;
    }

    uint64_t z_handler_JMP(z_op_t op1, z_op_t op2, z_op_t dest) {
        return 1; // always take the jump
    }

    uint64_t z_handler_JMP_EQ(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        return (v1->arithmetic_int_value == v2->arithmetic_int_value);
    }

    uint64_t z_handler_JMP_NEQ(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        return (v1->arithmetic_int_value != v2->arithmetic_int_value);
    }

    uint64_t z_handler_JMP_TRUE(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        return v1->arithmetic_int_value;
    }

    uint64_t z_handler_JMP_FALSE(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        return !v1->arithmetic_int_value;
    }

    uint64_t z_handler_MOV(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = *OP1_PTR;
        return 0;
    }

    uint64_t z_handler_MOV_FNC(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = fvalue(op1.uint_vaLue, context_object);
        return 0;
    }

    uint64_t z_handler_MOV_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(op1.int_value);
        return 0;
    }

    uint64_t z_handler_MOV_NULL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = nvalue();
        return 0;
    }

    uint64_t z_handler_MOV_BOOLEAN(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = bvalue(op1.int_value);
        return 0;
    }

    uint64_t z_handler_MOV_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto bit_representation = op1.uint_vaLue;
        double value = *reinterpret_cast<double *>(&bit_representation);
        *DESTINATION_PTR = dvalue(value);
        return 0;
    }

    uint64_t z_handler_MOV_STRING(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto *data = op1.str_value;
        VM_DEBUG(("mov str, %s", op1.str_value->c_str()));
        auto *copy = new string(*data);
        *DESTINATION_PTR = svalue(copy);
        return 0;
    }

    uint64_t z_handler_CALL(z_op_t op1, z_op_t op2, z_op_t dest) {
        VM_DEBUG(("call, bp: %d, sp: %d", base_pointer, stack_pointer));
        z_value_t &callee = context_object[op1.uint_vaLue];
        auto *fnc_ref = (z_fnc_ref_t *) callee.ptr_value;
        if (object_manager_is_null(callee)) {
            vm_log.error("null pointer exception: callee address was null");
            exit(1);
        }
        // push number of params pushed to stack
        push(uvalue(op2.uint_vaLue));
        // push current context pointer
        push(pvalue(context_object));
        // push requested return index
        push(uvalue(dest.uint_vaLue));
        // push parent context ptr;
        push(pvalue(fnc_ref->parent_context_ptr));

        return (uintptr_t) fnc_ref->instruction_index;
    }

    uint64_t z_handler_CALL_NATIVE(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto native_handler = get_native_fnc_at(OP1_PTR->uint_value);
        *DESTINATION_PTR = native_handler();
        return 0;
    }

    uint64_t z_handler_ADD_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                OP1_PTR->arithmetic_int_value + OP2_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_ADD_STRING(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto str1 = OP1_PTR->string_value;
        auto str2 = OP2_PTR->string_value;
        *DESTINATION_PTR = svalue(new string(*str1 + *str2));
        return 0;
    }

    uint64_t z_handler_ADD_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                OP1_PTR->arithmetic_decimal_value + OP2_PTR->arithmetic_decimal_value);
        return 0;
    }

    uint64_t z_handler_SUB_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                OP1_PTR->arithmetic_int_value - OP2_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_SUB_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                OP1_PTR->arithmetic_decimal_value - OP2_PTR->arithmetic_decimal_value);
        return 0;
    }

    uint64_t z_handler_DIV_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                OP1_PTR->arithmetic_int_value / OP2_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_DIV_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                OP1_PTR->arithmetic_decimal_value / OP2_PTR->arithmetic_decimal_value);
        return 0;
    }

    uint64_t z_handler_MUL_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                OP1_PTR->arithmetic_int_value *
                OP2_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_MUL_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                OP1_PTR->arithmetic_decimal_value *
                OP2_PTR->arithmetic_decimal_value);
        return 0;
    }

    uint64_t z_handler_MOD_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                OP1_PTR->arithmetic_int_value % OP2_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_MOD_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                fmod(OP1_PTR->arithmetic_decimal_value, OP2_PTR->arithmetic_decimal_value));
        return 0;
    }

    uint64_t z_handler_CMP_EQ(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value == v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_NEQ(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value != v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_GT_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value > v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_GT_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_decimal_value > v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_LT_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value < v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_LT_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_decimal_value < v2->arithmetic_decimal_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_GTE_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value >= v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_GTE_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_decimal_value >= v2->arithmetic_decimal_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_LTE_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_int_value <= v2->arithmetic_int_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CMP_LTE_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto v1 = OP1_PTR;
        auto v2 = OP2_PTR;
        auto ret = v1->arithmetic_decimal_value <= v2->arithmetic_decimal_value;
        *DESTINATION_PTR = bvalue(ret);
        return ret;
    }

    uint64_t z_handler_CAST_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(
                (float) OP1_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_NEG_INT(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = ivalue(
                -1 * OP1_PTR->arithmetic_int_value);
        return 0;
    }

    uint64_t z_handler_NEG_DECIMAL(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = dvalue(-1 * OP1_PTR->arithmetic_decimal_value);
        return 0;
    }

    uint64_t z_handler_PUSH(z_op_t op1, z_op_t op2, z_op_t dest) {
        push(*OP1_PTR);
        return 0;
    }

    uint64_t z_handler_POP(z_op_t op1, z_op_t op2, z_op_t dest) {
        *DESTINATION_PTR = pop();
        return 0;
    }

    uint64_t z_handler_ARG_READ(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto argNumber = op1.int_value;
        *DESTINATION_PTR = value_stack[base_pointer - 5 - argNumber]; // 5 is because of the calling convention
        return 0;
    }

    uint64_t z_handler_GET_IN_PARENT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto depth = op1.uint_vaLue;
        auto index = op2.uint_vaLue;
        auto parent_context = context_object;
        for (int i = 0; i < depth; i++) {
            parent_context = static_cast<z_value_t *>(parent_context[0].ptr_value);
        }
        *DESTINATION_PTR = parent_context[index];
        return 0;
    }

    uint64_t z_handler_GET_IN_OBJECT(z_op_t op1, z_op_t op2, z_op_t dest) {
        return 0;
    }

    uint64_t z_handler_SET_IN_PARENT(z_op_t op1, z_op_t op2, z_op_t dest) {
        auto depth = op1.uint_vaLue;
        auto index = op2.uint_vaLue;
        auto parent_context = context_object;
        for (int i = 0; i < depth; i++) {
            parent_context = static_cast<z_value_t *>(parent_context[0].ptr_value);
        }
        parent_context[dest.uint_vaLue] = context_object[index];
        return 0;
    }

    uint64_t z_handler_SET_IN_OBJECT(z_op_t op1, z_op_t op2, z_op_t dest) {
        return 0;
    }

    uint64_t z_handler_RET(z_op_t op1, z_op_t op2, z_op_t dest) {
        call_depth--;
        if (call_depth == 0) {
            VM_DEBUG(("root function returned, vm exited"));
            return 0;
        }

        stack_pointer = base_pointer;
        base_pointer = pop().uint_value;

        auto return_index_in_parent = pop().uint_value;
        auto return_index_in_current = dest.uint_vaLue;
        auto current_context_object = context_object;
        context_object = static_cast<z_value_t *>(pop().ptr_value);
        auto parent_context_object = context_object;
        if (return_index_in_current) {
            // move return value
            parent_context_object[return_index_in_parent] = current_context_object[return_index_in_current];
        }
        auto number_of_params_pushed_to_stack = pop().uint_value;
        stack_pointer -= number_of_params_pushed_to_stack;
        return 0;
    }

    uint64_t (*func_ptrs[])(z_op_t, z_op_t, z_op_t) =
            {z_handler_FN_ENTER_HEAP, z_handler_FN_ENTER_STACK,
             z_handler_JMP, z_handler_JMP_TRUE, z_handler_JMP_FALSE,
             z_handler_MOV, z_handler_MOV_FNC, z_handler_MOV_INT, z_handler_MOV_NULL,
             z_handler_MOV_BOOLEAN, z_handler_MOV_DECIMAL, z_handler_MOV_STRING,
             z_handler_CALL, z_handler_CALL_NATIVE,
             z_handler_ADD_INT, z_handler_ADD_STRING,
             z_handler_ADD_DECIMAL, z_handler_SUB_INT,
             z_handler_SUB_DECIMAL, z_handler_DIV_INT,
             z_handler_DIV_DECIMAL,
             z_handler_MUL_INT, z_handler_MUL_DECIMAL,
             z_handler_MOD_INT, z_handler_MOD_DECIMAL,
             z_handler_CMP_EQ,
             z_handler_CMP_NEQ, z_handler_CMP_GT_INT,
             z_handler_CMP_GT_DECIMAL, z_handler_CMP_LT_INT,
             z_handler_CMP_LT_DECIMAL,
             z_handler_CMP_GTE_INT, z_handler_CMP_GTE_DECIMAL,
             z_handler_CMP_LTE_INT,
             z_handler_CMP_LTE_DECIMAL, z_handler_CAST_DECIMAL,
             z_handler_NEG_INT, z_handler_NEG_DECIMAL,
             z_handler_PUSH, z_handler_POP, z_handler_ARG_READ,
             z_handler_GET_IN_PARENT,
             z_handler_GET_IN_OBJECT, z_handler_SET_IN_PARENT,
             z_handler_SET_IN_OBJECT, z_handler_RET};

    void vm_run(Program *program) {
        base_pointer = stack_pointer;
        push(pvalue(nullptr));
        init_native_functions();
        z_jit_fnc fnc = baseline_jit(program, func_ptrs);
        fnc();
    }
}