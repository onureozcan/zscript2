#pragma once

#include <string>

using namespace std;

namespace zero {

    enum Opcode {
        NO_OPCODE,
        LABEL,
        FN_ENTER_HEAP,
        FN_ENTER_STACK,
        JMP,
        JMP_EQ,
        JMP_NEQ,
        JMP_TRUE,
        JMP_FALSE,
        MOV,
        MOV_FNC,
        MOV_INT,
        MOV_BOOLEAN,
        MOV_DECIMAL,
        MOV_STRING,
        CALL,
        CALL_NATIVE,
        ADD_INT,
        ADD_STRING,
        ADD_DECIMAL,
        SUB_INT,
        SUB_DECIMAL,
        DIV_INT,
        DIV_DECIMAL,
        MUL_INT,
        MUL_DECIMAL,
        MOD_INT,
        MOD_DECIMAL,
        CMP_EQ,
        CMP_NEQ,
        CMP_GT_INT,
        CMP_GT_DECIMAL,
        CMP_LT_INT,
        CMP_LT_DECIMAL,
        CMP_GTE_INT,
        CMP_GTE_DECIMAL,
        CMP_LTE_INT,
        CMP_LTE_DECIMAL,
        CAST_DECIMAL,
        NEG_INT,
        NEG_DECIMAL,
        PUSH,
        POP,
        ARG_READ, // read an argument from the stack. pop wont work here because we need to take element relative to the base pointer
        GET_IN_PARENT, // to get an index in a parent context into current context. op1: depth, op2: index
        SET_IN_PARENT, // to set an index in a parent context from current context. op1: depth, op2: current value index, dest: index at parent
        GET_IN_OBJECT, // to get an index in a an object into current context. op1: object index in current context, op2: index
        SET_IN_OBJECT, // to set an index in a an object from current context. op1: object index in current context, op2: value in current context, dest: index at object
        RET
    };

    class Instruction {
    public:

        class Impl;

        unsigned int opCode = 0;
        union {
            unsigned int operand1 = 0;
            string *operand1AsLabel;
            double operand1AsDecimal;
        };
        union {
            unsigned int operand2 = 0;
        };
        union {
            string *destinationAsLabel;
            unsigned int destination = 0;
        };
        string comment;

        Instruction();

        Instruction *withOpCode(unsigned int opCode);

        Instruction *withOp1(unsigned int op);

        Instruction *withOp1(string *label);

        Instruction *withOp1(float decimal);

        Instruction *withOp2(unsigned int op);

        Instruction *withDestination(unsigned int dest);

        Instruction *withDestination(string* dest);

        Instruction *withComment(string comment);

        string toString() const;

    private:
        Impl *impl;
    };

    class Program {
    public:
        class Impl;

        explicit Program(string fileName);

        void addInstruction(Instruction *instruction);

        // insert at a specific position
        void addInstructionAt(Instruction *instruction, string label);

        void addLabel(string *label);

        void merge(Program *another);

        string toString();

        char *toBytes();

    private:
        Impl *impl;
    };
}