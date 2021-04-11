#pragma once

#include <string>

using namespace std;

namespace zero {

    enum OpType {
        INT, DECIMAL, FNC, NATIVE, ANY, STRING, NA
    };

    enum Opcode {
        LABEL,
        FN_ENTER,
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
        unsigned short opCode;
        unsigned short opType;
        union {
            unsigned int operand1;
            void *operand1AsPtr;
        };
        union {
            unsigned int operand2;
            void *operand2AsPtr;
        };
        union {
            unsigned int destination;
            void *destinationAsPtr;
        };
        string comment;
    };

    class Program {
    public:
        class Impl;

        Program(string fileName);

        void addInstruction(Instruction instruction);

        // insert at a specific position
        void addInstructionAt(Instruction instruction, string label);

        void addLabel(string label);

        void addConstant(string constant);

        void merge(Program *another);

        string toString();

    private:
        Impl *impl;
    };
}