#include <vm/vm.h>
#include <vm/object_manager.h>
#include <vm/shared.h>

#include <common/util.h>

#include <cmath>

#define GOTO_NEXT goto *(++instruction_ptr)->branch_addr
#define GOTO_CURRENT goto *(instruction_ptr)->branch_addr

//#define VM_DEBUG_ACTIVE

#ifdef VM_DEBUG_ACTIVE
#define VM_DEBUG(ARGS) vm_log.debug ARGS
#else
#define VM_DEBUG(ARGS)
#endif

#define DESTINATION_PTR ((z_value_t*)((uintptr_t)context_object + instruction_ptr->destination))
#define OP1_PTR ((z_value_t*)((uintptr_t)context_object + instruction_ptr->op1))
#define OP2_PTR ((z_value_t*)((uintptr_t)context_object + instruction_ptr->op2))

#include "vm_shared_inline.cpp"

using namespace std;

namespace zero {

    vm_instruction_t *prepare_vm_instructions(Program *program, void **labels) {
        auto *bytes = (uint64_t *) program->toBytes();
        uint64_t count = bytes[0];
        bytes++;
        auto *instruction = (vm_instruction_t *) bytes;
        for (int i = 0; i < count; i++) {
            auto opcode = instruction->opcode;
            auto is_immediate = opcode == MOV_STRING ||
                                opcode == MOV_BOOLEAN ||
                                opcode == MOV_INT ||
                                opcode == MOV_DECIMAL ||
                                opcode == MOV_FNC ||
                                opcode == FN_ENTER_HEAP ||
                                opcode == FN_ENTER_STACK ||
                                opcode == CALL ||
                                opcode == SET_IN_PARENT ||
                                opcode == GET_IN_PARENT ||
                                opcode == SET_IN_OBJECT ||
                                opcode == GET_IN_OBJECT ||
                                opcode == ARG_READ ||
                                opcode == RET;

            auto is_fn_enter = opcode <= FN_ENTER_HEAP;
            auto is_jmp = !is_fn_enter && opcode <= JMP_FALSE;
            auto is_using_destination_offset = opcode > JMP_FALSE && opcode < SET_IN_PARENT;

            if (is_jmp) {
                // jmp address pre-calculate
                instruction->destination = (uint64_t) (&((vm_instruction_t *) bytes)[instruction->destination]);
            }

            if (is_using_destination_offset) {
                // destination offset pre-calculate
                instruction->destination *= sizeof(z_value_t);
            }

            if (!is_immediate) {
                // value offset pre-calculate
                instruction->op1 *= sizeof(z_value_t);
                instruction->op2 *= sizeof(z_value_t);
            }

            // set branch address
            instruction->branch_addr = labels[instruction->opcode - 2]; // because first 2 opcodes are useless
            instruction++;
        }
        return (vm_instruction_t *) bytes;
    }

    void vm_run(Program *program) {
        static void *labels[] = {
                &&FN_ENTER_HEAP, &&FN_ENTER_STACK, &&JMP, &&JMP_TRUE, &&JMP_FALSE,
                &&MOV, &&MOV_FNC, &&MOV_INT, &&MOV_BOOLEAN,
                &&MOV_DECIMAL, &&MOV_STRING, &&CALL, &&CALL_NATIVE, &&ADD_INT, &&ADD_STRING,
                &&ADD_DECIMAL, &&SUB_INT, &&SUB_DECIMAL, &&DIV_INT, &&DIV_DECIMAL,
                &&MUL_INT, &&MUL_DECIMAL, &&MOD_INT, &&MOD_DECIMAL, &&CMP_EQ,
                &&CMP_NEQ, &&CMP_GT_INT, &&CMP_GT_DECIMAL, &&CMP_LT_INT, &&CMP_LT_DECIMAL,
                &&CMP_GTE_INT, &&CMP_GTE_DECIMAL, &&CMP_LTE_INT, &&CMP_LTE_DECIMAL, &&CAST_DECIMAL,
                &&NEG_INT, &&NEG_DECIMAL, &&PUSH, &&POP, &&ARG_READ, &&GET_IN_PARENT,
                &&GET_IN_OBJECT, &&SET_IN_PARENT, &&SET_IN_OBJECT, &&RET
        };

        vm_instruction_t *instructions = prepare_vm_instructions(program, labels);
        vm_instruction_t *instruction_ptr = instructions;

        z_value_t *context_object = nullptr; // function local variables are found in here, initially null

        int64_t base_pointer = stack_pointer;
        uint64_t call_depth = 0;

        push(pvalue(nullptr)); // first parent context is null
        init_native_functions();

        GOTO_CURRENT;

        FN_ENTER_HEAP:
        {
            unsigned int local_values_size = instruction_ptr->op1;
            auto parent_context = (z_value_t *) pop().ptr_value;
            context_object = alloc(local_values_size);
            init_call_context(context_object, parent_context);

            VM_DEBUG(("function enter heap, ip: %d, bp: %d, sp: %d", (instruction_ptr -
                                                                      instructions), base_pointer, stack_pointer));
            push(uvalue(base_pointer));
            base_pointer = stack_pointer;
            call_depth++;

            GOTO_NEXT;
        }
        FN_ENTER_STACK:
        {
            auto parent_context = (z_value_t *) pop().ptr_value;
            VM_DEBUG(("function enter stack, ip: %d, bp: %d, sp: %d", (instruction_ptr -
                                                                       instructions), base_pointer, stack_pointer));
            push(uvalue(base_pointer));
            base_pointer = stack_pointer;
            call_depth++;

            unsigned int local_values_size = instruction_ptr->op1;

            context_object = &value_stack[stack_pointer];
            init_call_context(context_object, parent_context);

            stack_pointer += local_values_size;
            if (stack_pointer > STACK_MAX) {
                vm_log.error("could not allocate local stack frame, stack overflow!");
                exit(1);
            }

            VM_DEBUG(("stack allocated %d, sp: %d", local_values_size, stack_pointer));

            GOTO_NEXT;
        }
        JMP:
        {
            instruction_ptr = (vm_instruction_t *) (instruction_ptr->destination);
            GOTO_CURRENT;
        }
        JMP_TRUE:
        {
            auto v1 = OP1_PTR;
            if (v1->arithmetic_int_value) {
                instruction_ptr = (vm_instruction_t *) (instruction_ptr->destination);
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_FALSE:
        {
            auto v1 = OP1_PTR;
            if (!v1->arithmetic_int_value) {
                instruction_ptr = (vm_instruction_t *) (instruction_ptr->destination);
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        MOV:
        {
            *DESTINATION_PTR = *OP1_PTR;
            GOTO_NEXT;
        }
        MOV_FNC:
        {
            *DESTINATION_PTR = fvalue(instruction_ptr->op1, context_object);
            GOTO_NEXT;
        }
        MOV_INT:
        {
            *DESTINATION_PTR = ivalue(instruction_ptr->op1);
            GOTO_NEXT;
        }
        MOV_BOOLEAN:
        {
            *DESTINATION_PTR = bvalue(instruction_ptr->op1);
            GOTO_NEXT;
        }
        MOV_DECIMAL:
        {
            double value = *(double *) &instruction_ptr->op1;
            *DESTINATION_PTR = dvalue(value);
            GOTO_NEXT;
        }
        MOV_STRING:
        {
            auto *data = instruction_ptr->op1_string;
            auto *copy = new string(*data);
            *DESTINATION_PTR = svalue(copy);
            GOTO_NEXT;
        }
        CALL:
        {
            VM_DEBUG(("call, ip: %d, bp: %d, sp: %d", (instruction_ptr - instructions), base_pointer, stack_pointer));
            auto *fnc_ref = (z_fnc_ref_t *) context_object[instruction_ptr->op1].ptr_value;
            // push number of params pushed to stack
            push(uvalue(instruction_ptr->op2));
            // push current instruction pointer
            push(pvalue(instruction_ptr + 1));
            // push current context pointer
            push(pvalue(context_object));
            // push requested return index
            push(uvalue(instruction_ptr->destination));
            // push parent context ptr;
            push(pvalue(fnc_ref->parent_context_ptr));

            instruction_ptr = instructions + (fnc_ref->instruction_index);
            GOTO_CURRENT;
        }
        CALL_NATIVE:
        {
            auto native_handler = get_native_fnc_at(OP1_PTR->uint_value);
            *DESTINATION_PTR = native_handler();
            GOTO_NEXT;
        }
        ADD_INT:
        {
            *DESTINATION_PTR = ivalue(
                    OP1_PTR->arithmetic_int_value + OP2_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        ADD_STRING:
        {
            auto str1 = OP1_PTR->string_value;
            auto str2 = OP2_PTR->string_value;
            *DESTINATION_PTR = svalue(new string(*str1 + *str2));
            GOTO_NEXT;
        }
        ADD_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    OP1_PTR->arithmetic_decimal_value + OP2_PTR->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        SUB_INT:
        {
            *DESTINATION_PTR = ivalue(
                    OP1_PTR->arithmetic_int_value - OP2_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        SUB_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    OP1_PTR->arithmetic_decimal_value - OP2_PTR->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        DIV_INT:
        {
            *DESTINATION_PTR = ivalue(
                    OP1_PTR->arithmetic_int_value / OP2_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        DIV_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    OP1_PTR->arithmetic_decimal_value / OP2_PTR->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        MUL_INT:
        {
            *DESTINATION_PTR = ivalue(
                    OP1_PTR->arithmetic_int_value *
                    OP2_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        MUL_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    OP1_PTR->arithmetic_decimal_value *
                    OP2_PTR->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        MOD_INT:
        {
            *DESTINATION_PTR = ivalue(
                    OP1_PTR->arithmetic_int_value % OP2_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        MOD_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    fmod(OP1_PTR->arithmetic_decimal_value, OP2_PTR->arithmetic_decimal_value));
            GOTO_NEXT;
        }
        CMP_EQ:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value == v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_NEQ:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value != v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_GT_INT:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value > v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_GT_DECIMAL:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(
                    v1->arithmetic_decimal_value > v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LT_INT:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value < v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LT_DECIMAL:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(
                    v1->arithmetic_decimal_value < v2->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CMP_GTE_INT:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value >= v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_GTE_DECIMAL:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(
                    v1->arithmetic_decimal_value >= v2->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CMP_LTE_INT:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(v1->arithmetic_int_value <= v2->arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LTE_DECIMAL:
        {
            auto v1 = OP1_PTR;
            auto v2 = OP2_PTR;
            *DESTINATION_PTR = bvalue(
                    v1->arithmetic_decimal_value <= v2->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CAST_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(
                    (float) OP1_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        NEG_INT:
        {
            *DESTINATION_PTR = ivalue(
                    -1 * OP1_PTR->arithmetic_int_value);
            GOTO_NEXT;
        }
        NEG_DECIMAL:
        {
            *DESTINATION_PTR = dvalue(-1 * OP1_PTR->arithmetic_decimal_value);
            GOTO_NEXT;
        }
        PUSH:
        {
            push(*OP1_PTR);
            GOTO_NEXT;
        }
        POP:
        {
            *DESTINATION_PTR = pop();
            GOTO_NEXT;
        }
        ARG_READ:
        {
            auto argNumber = instruction_ptr->op1;
            *DESTINATION_PTR = value_stack[base_pointer - 6 - argNumber]; // 6 is because of the calling convention
            GOTO_NEXT;
        }
        GET_IN_PARENT:
        {
            auto depth = instruction_ptr->op1;
            auto index = instruction_ptr->op2;
            auto parent_context = context_object;
            for (int i = 0; i < depth; i++) {
                parent_context = static_cast<z_value_t *>(parent_context[0].ptr_value);
            }
            *DESTINATION_PTR = parent_context[index];
            GOTO_NEXT;
        }
        GET_IN_OBJECT:
        { GOTO_NEXT; }
        SET_IN_PARENT:
        {
            auto depth = instruction_ptr->op1;
            auto index = instruction_ptr->op2;
            auto parent_context = context_object;
            for (int i = 0; i < depth; i++) {
                parent_context = static_cast<z_value_t *>(parent_context[0].ptr_value);
            }
            parent_context[instruction_ptr->destination] = context_object[index];
            GOTO_NEXT;
        }
        SET_IN_OBJECT:
        { GOTO_NEXT; }
        RET:
        {
            call_depth--;
            if (call_depth == 0) {
                VM_DEBUG(("root function returned, vm exited"));
                return; // this means the root function returned
            }

            stack_pointer = base_pointer;
            base_pointer = pop().uint_value;

            auto return_index_in_parent = pop().uint_value;
            auto return_index_in_current = instruction_ptr->destination;
            auto current_context_object = context_object;
            context_object = static_cast<z_value_t *>(pop().ptr_value);
            instruction_ptr = static_cast<vm_instruction_t *>(pop().ptr_value);
            auto parent_context_object = context_object;
            if (return_index_in_current) {
                // move return value
                *(z_value_t *) (((uintptr_t) parent_context_object) + return_index_in_parent) =
                        current_context_object[return_index_in_current];
            }
            auto number_of_params_pushed_to_stack = pop().uint_value;
            stack_pointer -= number_of_params_pushed_to_stack;
            VM_DEBUG(
                    ("ret, next_ip: %d, sp: %d, bp:%d", (instruction_ptr - instructions), stack_pointer, base_pointer));
            GOTO_CURRENT;
        }
    }
}