#include <vm/vm.h>
#include <vm/object_manager.h>

#include <common/util.h>

#include <cmath>
#include <iostream>

#define GOTO_NEXT goto *(++instruction_ptr)->branch_addr
#define GOTO_CURRENT goto *(instruction_ptr)->branch_addr

//#define VM_DEBUG_ACTIVE

#ifdef VM_DEBUG_ACTIVE
#define VM_DEBUG(ARGS) vm_log.debug ARGS
#else
#define VM_DEBUG(ARGS)
#endif

using namespace std;

namespace zero {

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

    Logger vm_log("vm");
    vector<z_native_fnc_t> native_function_map = {native_print};

    // for parameter passing, return address etc
    int64_t stack_pointer = 0;
    z_value_t value_stack[STACK_MAX];

    inline z_value_t *alloc(unsigned int size) {
        auto *ptr = static_cast<z_value_t *>(malloc_aligned(size * sizeof(z_value_t)));
        if (ptr == nullptr) {
            vm_log.error("could not allocate %d size frame!", size);
            exit(1);
        }
        return ptr;
    }

    inline z_value_t uvalue(uint64_t _val) {
        z_value_t val;
        val.uint_value = _val;
        return val;
    }

    inline z_value_t ivalue(int32_t _val) {
        z_value_t val;
        val.arithmetic_int_value = _val;
        val.primitive_type = PRIMITIVE_TYPE_INT;
        return val;
    }

    inline z_value_t bvalue(int32_t _val) {
        z_value_t val;
        val.arithmetic_int_value = _val;
        val.primitive_type = PRIMITIVE_TYPE_BOOLEAN;
        return val;
    }

    inline z_value_t dvalue(float _val) {
        z_value_t val;
        val.arithmetic_decimal_value = _val;
        val.primitive_type = PRIMITIVE_TYPE_DOUBLE;
        return val;
    }

    inline z_value_t pvalue(void *_val) {
        z_value_t val;
        val.ptr_value = _val;
        return val;
    }

    inline z_value_t svalue(string *_val) {
        z_value_t val;
        val.string_value = _val;
        object_manager_register_string(val);
        return val;
    }

    inline z_value_t fvalue(unsigned int instruction_index, z_value_t *context_object) {
        z_value_t val;
        val.ptr_value = object_manager_create_fn_ref(instruction_index, context_object);
        return val;
    }

    z_value_t native_print() {
        z_value_t z_value = pop();
        z_object_type_info type = object_manager_guess_type(z_value);
        string str_value;
        switch (type) {
            case VM_VALUE_TYPE_STRING:
                str_value = *z_value.string_value;
                break;
            case VM_VALUE_TYPE_DECIMAL:
                str_value = to_string(z_value.arithmetic_decimal_value);
                break;
            case VM_VALUE_TYPE_BOOLEAN:
                str_value = z_value.arithmetic_int_value ? "true" : "false";
                break;
            case VM_VALUE_TYPE_INT:
                str_value = to_string(z_value.arithmetic_int_value);
                break;
            case VM_VALUE_TYPE_FUNCTION_REF:
                str_value = "[function ref]";
                break;
            case VM_VALUE_TYPE_TYPE_OBJECT:
                str_value = "[object ref]";
                break;
            default:
                str_value = "[?]";
                break;
        }
        cout << str_value << "\n";
        return ivalue(0);
    }

    inline void push(z_value_t value) {
        if (stack_pointer > STACK_MAX) {
            vm_log.error("stack overflow!", stack_pointer);
            exit(1);
        }
        value_stack[stack_pointer] = value;
        stack_pointer++;
    }

    inline z_value_t pop() {
        stack_pointer--;
        if (stack_pointer < 0) {
            vm_log.error("stack underflow!", stack_pointer);
            exit(1);
        }
        auto ret = value_stack[stack_pointer];
        return ret;
    }

    vm_instruction_t *get_vm_instructions(Program *program, void *labels[]) {
        auto *bytes = (uint64_t *) program->toBytes();
        uint64_t count = bytes[0];
        bytes++;
        auto *instruction = (vm_instruction_t *) bytes;
        for (int i = 0; i < count; i++) {
            instruction->branch_addr = labels[instruction->opcode - 2]; // because first 2 opcodes are useless
            instruction++;
        }
        return (vm_instruction_t *) bytes;
    }

    inline void init_call_context(z_value_t *context_object, z_value_t *parent_context) {
        context_object[0] = pvalue(parent_context); // 0th index is a pointer to parent
        if (parent_context == nullptr) {
            // init native functions
            for (unsigned int i = 0; i < native_function_map.size(); i++) {
                context_object[i + 1] = uvalue(i);
            }
        }
    }

    void vm_run(Program *program) {
        static void *labels[] = {
                &&FN_ENTER_HEAP, &&FN_ENTER_STACK, &&JMP, &&JMP_EQ, &&JMP_NEQ,
                &&JMP_GT_INT, &&JMP_GT_DECIMAL, &&JMP_LT_INT, &&JMP_LT_DECIMAL, &&JMP_GTE_INT,
                &&JMP_GTE_DECIMAL, &&JMP_LTE_INT, &&JMP_LTE_DECIMAL, &&MOV, &&MOV_FNC, &&MOV_INT,
                &&MOV_DECIMAL, &&MOV_STRING, &&CALL, &&CALL_NATIVE, &&ADD_INT, &&ADD_STRING,
                &&ADD_DECIMAL, &&SUB_INT, &&SUB_DECIMAL, &&DIV_INT, &&DIV_DECIMAL,
                &&MUL_INT, &&MUL_DECIMAL, &&MOD_INT, &&MOD_DECIMAL, &&CMP_EQ,
                &&CMP_NEQ, &&CMP_GT_INT, &&CMP_GT_DECIMAL, &&CMP_LT_INT, &&CMP_LT_DECIMAL,
                &&CMP_GTE_INT, &&CMP_GTE_DECIMAL, &&CMP_LTE_INT, &&CMP_LTE_DECIMAL, &&CAST_DECIMAL,
                &&NEG_INT, &&NEG_DECIMAL, &&PUSH, &&POP, &&ARG_READ, &&GET_IN_PARENT, &&SET_IN_PARENT,
                &&GET_IN_OBJECT, &&SET_IN_OBJECT, &&RET
        };

        vm_instruction_t *instructions = get_vm_instructions(program, labels);
        vm_instruction_t *instruction_ptr = instructions;

        z_value_t *context_object = nullptr; // function local variables are found in here, initially null

        int64_t base_pointer = stack_pointer;
        uint64_t call_depth = 0;

        push(pvalue(nullptr)); // first parent context is null

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
            instruction_ptr = &instructions[instruction_ptr->destination];
            GOTO_CURRENT;
        }
        JMP_EQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.uint_value == v2.uint_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_NEQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.uint_value != v2.uint_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_int_value > v2.arithmetic_int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_decimal_value > v2.arithmetic_decimal_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_int_value < v2.arithmetic_int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_decimal_value < v2.arithmetic_decimal_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_int_value >= v2.arithmetic_int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_decimal_value >= v2.arithmetic_decimal_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_int_value <= v2.arithmetic_int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.arithmetic_decimal_value <= v2.arithmetic_decimal_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        MOV:
        {
            context_object[instruction_ptr->destination] = context_object[instruction_ptr->op1];
            GOTO_NEXT;
        }
        MOV_FNC:
        {
            context_object[instruction_ptr->destination] = fvalue(instruction_ptr->op1, context_object);
            GOTO_NEXT;
        }
        MOV_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(instruction_ptr->op1);
            GOTO_NEXT;
        }
        MOV_DECIMAL:
        {
            double value = *(double *) &instruction_ptr->op1;
            context_object[instruction_ptr->destination] = dvalue(value);
            GOTO_NEXT;
        }
        MOV_STRING:
        {
            auto *data = instruction_ptr->op1_string;
            auto *copy = new string(*data);
            context_object[instruction_ptr->destination] = svalue(copy);
            GOTO_NEXT;
        }
        CALL:
        {
            auto *fnc_ref = (z_fnc_ref_t *) context_object[instruction_ptr->op1].ptr_value;

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
            auto native_handler = native_function_map[context_object[instruction_ptr->op1].uint_value];
            context_object[instruction_ptr->destination] = native_handler();
            GOTO_NEXT;
        }
        ADD_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    context_object[instruction_ptr->op1].arithmetic_int_value +
                    context_object[instruction_ptr->op2].arithmetic_int_value);
            GOTO_NEXT;
        }
        ADD_STRING:
        {
            auto str1 = context_object[instruction_ptr->op1].string_value;
            auto str2 = context_object[instruction_ptr->op2].string_value;
            context_object[instruction_ptr->destination] = svalue(new string(*str1 + *str2));
            GOTO_NEXT;
        }
        ADD_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    context_object[instruction_ptr->op1].arithmetic_decimal_value +
                    context_object[instruction_ptr->op2].arithmetic_decimal_value);
            GOTO_NEXT;
        }
        SUB_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    context_object[instruction_ptr->op1].arithmetic_int_value -
                    context_object[instruction_ptr->op2].arithmetic_int_value);
            GOTO_NEXT;
        }
        SUB_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    context_object[instruction_ptr->op1].arithmetic_decimal_value -
                    context_object[instruction_ptr->op2].arithmetic_decimal_value);
            GOTO_NEXT;
        }
        DIV_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    context_object[instruction_ptr->op1].arithmetic_int_value /
                    context_object[instruction_ptr->op2].arithmetic_int_value);
            GOTO_NEXT;
        }
        DIV_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    context_object[instruction_ptr->op1].arithmetic_decimal_value /
                    context_object[instruction_ptr->op2].arithmetic_decimal_value);
            GOTO_NEXT;
        }
        MUL_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    context_object[instruction_ptr->op1].arithmetic_int_value *
                    context_object[instruction_ptr->op2].arithmetic_int_value);
            GOTO_NEXT;
        }
        MUL_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    context_object[instruction_ptr->op1].arithmetic_decimal_value *
                    context_object[instruction_ptr->op2].arithmetic_decimal_value);
            GOTO_NEXT;
        }
        MOD_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    context_object[instruction_ptr->op1].arithmetic_int_value %
                    context_object[instruction_ptr->op2].arithmetic_int_value);
            GOTO_NEXT;
        }
        MOD_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    fmod(context_object[instruction_ptr->op1].arithmetic_decimal_value,
                         context_object[instruction_ptr->op1].arithmetic_decimal_value));
            GOTO_NEXT;
        }
        CMP_EQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.uint_value == v2.uint_value);
            GOTO_NEXT;
        }
        CMP_NEQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.uint_value != v2.uint_value);
            GOTO_NEXT;
        }
        CMP_GT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.arithmetic_int_value > v2.arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_GT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(
                    v1.arithmetic_decimal_value > v2.arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.arithmetic_int_value < v2.arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(
                    v1.arithmetic_decimal_value < v2.arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CMP_GTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.arithmetic_int_value >= v2.arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_GTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(
                    v1.arithmetic_decimal_value >= v2.arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CMP_LTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(v1.arithmetic_int_value <= v2.arithmetic_int_value);
            GOTO_NEXT;
        }
        CMP_LTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = bvalue(
                    v1.arithmetic_decimal_value <= v2.arithmetic_decimal_value);
            GOTO_NEXT;
        }
        CAST_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    (float) context_object[instruction_ptr->op1].arithmetic_int_value);
            GOTO_NEXT;
        }
        NEG_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    -1 * context_object[instruction_ptr->op1].arithmetic_int_value);
            GOTO_NEXT;
        }
        NEG_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    -1 * context_object[instruction_ptr->op1].arithmetic_decimal_value);
            GOTO_NEXT;
        }
        PUSH:
        {
            push(context_object[instruction_ptr->op1]);
            GOTO_NEXT;
        }
        POP:
        {
            context_object[instruction_ptr->destination] = pop();
            GOTO_NEXT;
        }
        ARG_READ:
        {
            auto argNumber = instruction_ptr->op1;
            context_object[instruction_ptr->destination] =
                    value_stack[base_pointer - 5 - argNumber]; // 5 is because of the calling convention
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
            context_object[instruction_ptr->destination] = parent_context[index];
            GOTO_NEXT;
        }
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
        GET_IN_OBJECT:
        { GOTO_NEXT; }
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

            VM_DEBUG(("ret. sp: %d, bp:%d", stack_pointer, base_pointer));

            auto return_index_in_parent = pop().uint_value;
            auto return_index_in_current = instruction_ptr->destination;
            auto current_context_object = context_object;
            context_object = static_cast<z_value_t *>(pop().ptr_value);
            instruction_ptr = static_cast<vm_instruction_t *>(pop().ptr_value);
            auto parent_context_object = context_object;
            if (return_index_in_current) {
                // move return value
                parent_context_object[return_index_in_parent] = current_context_object[return_index_in_current];
            }
            GOTO_CURRENT;
        }
    }
}