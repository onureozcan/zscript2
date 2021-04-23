#include <vm/vm.h>
#include <common/logger.h>

#include <cmath>
#include <iostream>

#define GOTO_NEXT goto *(++instruction_ptr)->branch_addr
#define GOTO_CURRENT goto *(instruction_ptr)->branch_addr

//#define VM_DEBUG_ACTIVE

#ifdef VM_DEBUG_ACTIVE
#define VM_DEBUG(ARGS) log.debug ARGS
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
            uint64_t op1_decimal;
            string *op1_string;
        };
        uint64_t op2;
        uint64_t destination;
    } vm_instruction_t;

    Logger log("vm");
    vector<z_native_fnc_t> native_function_map = {native_print};

    // for parameter passing, return address etc
    int64_t stack_pointer = 0;
    z_value_t value_stack[STACK_MAX];

    inline z_value_t *alloc(unsigned int size) {
        auto *ptr = static_cast<z_value_t *>(malloc(size * sizeof(z_value_t)));
        if (ptr == nullptr) {
            log.error("could not allocate %d size frame!", size);
            exit(1);
        }
        return ptr;
    }

    inline z_value_t uvalue(unsigned int _val) {
        z_value_t val;
        val.uint_value = _val;
        return val;
    }

    inline z_value_t ivalue(int _val) {
        z_value_t val;
        val.int_value = _val;
        return val;
    }

    inline z_value_t dvalue(double _val) {
        z_value_t val;
        val.double_value = _val;
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
        return val;
    }

    inline z_value_t fvalue(uint64_t instruction_index, z_value_t *context_object) {
        auto fun_ref = (z_fnc_ref_t *) malloc(sizeof(z_fnc_ref_t));
        if (fun_ref == nullptr) {
            log.error("could not allocate memory for a function reference");
            exit(1);
        }
        fun_ref->parent_context_ptr = context_object;
        fun_ref->instruction_index = instruction_index;

        z_value_t val;
        val.ptr_value = fun_ref;
        return val;
    }

    z_value_t native_print() {
        string *value = pop().string_value;
        cout << *value << "\n";
        return ivalue(0);
    }

    inline void push(z_value_t value) {
        if (stack_pointer > STACK_MAX) {
            log.error("stack overflow!", stack_pointer);
            exit(1);
        }
        value_stack[stack_pointer] = value;
        stack_pointer++;
    }

    inline z_value_t pop() {
        stack_pointer--;
        if (stack_pointer < 0) {
            log.error("stack underflow!", stack_pointer);
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
                &&NEG_INT, &&NEG_DECIMAL, &&PUSH, &&POP, &&GET_IN_PARENT, &&SET_IN_PARENT,
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
                log.error("could not allocate local stack frame, stack overflow!");
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
            if (v1.int_value > v2.int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.double_value > v2.double_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.int_value < v2.int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.double_value < v2.double_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.int_value >= v2.int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_GTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.double_value >= v2.double_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.int_value <= v2.int_value) {
                instruction_ptr = &instructions[instruction_ptr->destination];
                GOTO_CURRENT;
            }
            GOTO_NEXT;
        }
        JMP_LTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            if (v1.double_value <= v2.double_value) {
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
            context_object[instruction_ptr->destination] = dvalue(instruction_ptr->op1_decimal);
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
            context_object[instruction_ptr->destination] = ivalue(context_object[instruction_ptr->op1].int_value +
                                                                  context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        ADD_STRING:
        {
            // TODO
            GOTO_NEXT;
        }
        ADD_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(context_object[instruction_ptr->op1].double_value +
                                                                  context_object[instruction_ptr->op1].double_value);
            GOTO_NEXT;
        }
        SUB_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(context_object[instruction_ptr->op1].int_value -
                                                                  context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        SUB_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(context_object[instruction_ptr->op1].double_value -
                                                                  context_object[instruction_ptr->op1].double_value);
            GOTO_NEXT;
        }
        DIV_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(context_object[instruction_ptr->op1].int_value /
                                                                  context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        DIV_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(context_object[instruction_ptr->op1].double_value /
                                                                  context_object[instruction_ptr->op1].double_value);
            GOTO_NEXT;
        }
        MUL_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(context_object[instruction_ptr->op1].int_value *
                                                                  context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        MUL_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(context_object[instruction_ptr->op1].double_value *
                                                                  context_object[instruction_ptr->op1].double_value);
            GOTO_NEXT;
        }
        MOD_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(context_object[instruction_ptr->op1].int_value %
                                                                  context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        MOD_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    fmod(context_object[instruction_ptr->op1].double_value,
                         context_object[instruction_ptr->op1].double_value));
            GOTO_NEXT;
        }
        CMP_EQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value == v2.int_value);
            GOTO_NEXT;
        }
        CMP_NEQ:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value != v2.int_value);
            GOTO_NEXT;
        }
        CMP_GT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value > v2.int_value);
            GOTO_NEXT;
        }
        CMP_GT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = dvalue(v1.double_value > v2.int_value);
            GOTO_NEXT;
        }
        CMP_LT_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value < v2.int_value);
            GOTO_NEXT;
        }
        CMP_LT_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = dvalue(v1.double_value < v2.double_value);
            GOTO_NEXT;
        }
        CMP_GTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value >= v2.int_value);
            GOTO_NEXT;
        }
        CMP_GTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = dvalue(v1.double_value >= v2.double_value);
            GOTO_NEXT;
        }
        CMP_LTE_INT:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = ivalue(v1.int_value <= v2.int_value);
            GOTO_NEXT;
        }
        CMP_LTE_DECIMAL:
        {
            auto v1 = context_object[instruction_ptr->op1];
            auto v2 = context_object[instruction_ptr->op2];
            context_object[instruction_ptr->destination] = dvalue(v1.double_value <= v2.double_value);
            GOTO_NEXT;
        }
        CAST_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    (double) context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        NEG_INT:
        {
            context_object[instruction_ptr->destination] = ivalue(
                    -1 * context_object[instruction_ptr->op1].int_value);
            GOTO_NEXT;
        }
        NEG_DECIMAL:
        {
            context_object[instruction_ptr->destination] = dvalue(
                    -1 * context_object[instruction_ptr->op1].double_value);
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
            base_pointer = pop().int_value;

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