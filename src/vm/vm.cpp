#include <vm/vm.h>
#include <common/logger.h>

#include <cstring>
#include <cmath>

#define GOTO_NEXT goto *(++instruction_ptr)->branch_addr
#define GOTO_CURRENT goto *(instruction_ptr)->branch_addr

using namespace std;

namespace zero {

    typedef struct {
        union {
            void *branch_addr;
            unsigned int opcode;
        };
        union {
            unsigned int op1;
            unsigned int op1_decimal;
            string *op1_string;
        };
        unsigned int op2;
        unsigned int destination;
    } vm_instruction_t;

    Logger log("vm");
    vector<z_native_fnc_t> native_function_map = {native_print};

    // for parameter passing, return address etc
    unsigned int sp = 0;
    z_value_t value_stack[STACK_MAX];

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

    inline z_value_t svalue(char *_val) {
        z_value_t val;
        val.string_value = _val;
        return val;
    }

    z_value_t native_print(z_value_t *stack, unsigned int *sp_ptr) {
        char* value = stack[*sp_ptr].string_value;
        printf("%s\n", value);
        *sp_ptr -= 1;
        return ivalue(0);
    }

    inline void push(z_value_t value) {
        sp++;
        if (sp > STACK_MAX) {
            log.error("stack overflow!", sp);
            exit(1);
        }
        value_stack[sp] = value;
    }

    inline z_value_t pop() {
        sp--;
        if (sp < 0) {
            log.error("stack underflow!", sp);
            exit(1);
        }
        return value_stack[sp];
    }

    inline z_value_t *alloc(unsigned int size) {
        auto *ptr = static_cast<z_value_t *>(malloc(size * sizeof(z_value_t)));
        if (ptr == nullptr) {
            log.error("could not allocate %d size frame!", size);
            exit(1);
        }
        return ptr;
    }

    vm_instruction_t *get_vm_instructions(Program *program, void *labels[]) {
        auto *bytes = (unsigned int *) program->toBytes();
        unsigned int count = bytes[0];
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

        GOTO_CURRENT;

        FN_ENTER_HEAP:
        {
            unsigned int local_values_size = instruction_ptr->op1;
            auto parent_context = context_object;
            context_object = alloc(local_values_size);
            init_call_context(context_object, parent_context);
            GOTO_NEXT;
        }
        FN_ENTER_STACK:
        {
            unsigned int local_values_size = instruction_ptr->op1;
            auto parent_context = context_object;
            context_object = &value_stack[sp];
            init_call_context(context_object, parent_context);

            sp += local_values_size;
            if (sp > STACK_MAX) {
                log.error("could not allocate local stack frame, stack overflow!");
                exit(1);
            }

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
            context_object[instruction_ptr->destination] = uvalue(instruction_ptr->op1);
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
            const char *data = instruction_ptr->op1_string->c_str();
            char *copy = strdup(data);
            context_object[instruction_ptr->destination] = svalue(copy);
            GOTO_NEXT;
        }
        CALL:
        {
            // push current instruction pointer
            push(pvalue(instruction_ptr + 1));
            // push requested return index
            push(uvalue(instruction_ptr->destination));

            instruction_ptr = instructions + (context_object[instruction_ptr->op1]).uint_value;
            GOTO_CURRENT;
        }
        CALL_NATIVE:
        {
            auto native_handler = native_function_map[context_object[instruction_ptr->op1].uint_value];
            context_object[instruction_ptr->destination] = native_handler(value_stack, &sp);
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
            auto return_index_in_parent = pop().uint_value;
            auto return_index_in_current = instruction_ptr->destination;
            instruction_ptr = static_cast<vm_instruction_t *>(pop().ptr_value);
            auto current_context_object = context_object;
            context_object = static_cast<z_value_t *>(context_object[0].ptr_value);
            if (return_index_in_current) {
                context_object[return_index_in_parent] = current_context_object[return_index_in_current];
            }
            if (context_object == nullptr) {
                exit(0);
            }
            GOTO_CURRENT;
        }
    }
}