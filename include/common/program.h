#pragma once

#include <string>

using namespace std;

namespace zero {

    enum OpType {
        INT, DECIMAL, FNC, NATIVE, ANY
    };

    enum Opcode {
        NOP,
        JMP,
        JMP_EQ,
        JMP_NEQ,
        JMP_GT,
        JMP_LT,
        JMP_GTE,
        JMP_LTE,
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
        unsigned int operand1;
        unsigned int operand2;
        unsigned int destination;
    };

    class Program {
    public:
        class Impl;

        Program(string fileName);

        void addInstruction(Instruction instruction);

        string toString();

    private:
        Impl *impl;
    };
}