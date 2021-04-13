#pragma once

#include <string>

using namespace std;

namespace zero {

    enum OpType {
        NO_TYPE, INT, DECIMAL, FNC, NATIVE, ANY, STRING
    };

    enum Opcode {
        NO_OPCODE,
        LABEL,
        FN_ENTER_HEAP,
        FN_ENTER_STACK,
        JMP,
        JMP_EQ,
        JMP_NEQ,
        JMP_GT,
        JMP_LT,
        JMP_GTE,
        JMP_LTE,
        MOV_I,
        MOV,
        CALL,
        ADD,
        SUB,
        DIV,
        MUL,
        MOD,
        CMP,
        CAST_F,
        RET
    };

    class Instruction {
    public:

        class Impl;

        unsigned short opCode = 0;
        unsigned short opType = 0;
        union {
            unsigned int operand1 = 0;
            string *operand1AsLabel;
            float operand1AsDecimal;
        };
        union {
            unsigned int operand2 = 0;
        };
        union {
            unsigned int destination = 0;
        };
        string comment = "";

        Instruction();

        Instruction *withOpCode(unsigned short opCode);

        Instruction *withOpType(unsigned short type);

        Instruction *withOp1(unsigned int op);

        Instruction *withOp1(string *label);

        Instruction *withOp1(float decimal);

        Instruction *withOp2(unsigned int op);

        Instruction *withDestination(unsigned int dest);

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

        void addConstant(string constant);

        void merge(Program *another);

        string toString();

    private:
        Impl *impl;
    };
}