#pragma once

#include <string>
#include <map>
#include <vector>

using namespace std;

namespace zero {

    enum Opcode {
        NO_OPCODE,
        LABEL,
        FN_ENTER_HEAP,
        FN_ENTER_STACK,
        JMP,
        JMP_TRUE,
        JMP_FALSE,
        MOV,
        MOV_FNC,
        MOV_INT,
        MOV_NULL,
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
        GET_IN_OBJECT, // to get an index in a an object into current context. op1: object index in current context, op2: index
        SET_IN_PARENT, // to set an index in a parent context from current context. op1: depth, op2: current value index, dest: index at parent
        SET_IN_OBJECT, // to set an index in a an object from current context. op1: object index in current context, op2: value in current context, dest: index at object
        RET
    };

    enum OpType {
        IMM_INT,
        IMM_DECIMAL,
        IMM_STRING,
        IMM_ADDRESS,
        INDEX,
        UNUSED
    };

    enum OpcodeType {
        FUNCTION_ENTER,
        JUMP,
        COMPARISON,
        OTHER
    };

    typedef struct {
        OpcodeType opcodeType;
        OpType op1Type;
        OpType op2Type;
        OpType destType;
    } InstructionDescriptor;

    static const map<int, InstructionDescriptor> instructionDescriptionTable = {
            {RET,             {OTHER,          UNUSED,      UNUSED,  IMM_INT}},
            {SET_IN_OBJECT,   {OTHER,          IMM_INT,     IMM_INT, INDEX}},
            {SET_IN_PARENT,   {OTHER,          IMM_INT,     INDEX,   INDEX}},
            {GET_IN_OBJECT,   {OTHER,          INDEX,       INDEX,   INDEX}},
            {GET_IN_PARENT,   {OTHER,          IMM_INT,     IMM_INT, INDEX}},
            {ARG_READ,        {OTHER,          IMM_INT,     UNUSED,  INDEX}},
            {PUSH,            {OTHER,          INDEX,       UNUSED,  UNUSED}},
            {POP,             {OTHER,          INDEX,       UNUSED,  UNUSED}},
            {NEG_DECIMAL,     {OTHER,          INDEX,       UNUSED,  INDEX}},
            {NEG_INT,         {OTHER,          INDEX,       UNUSED,  INDEX}},
            {CMP_EQ,          {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_NEQ,         {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_GT_INT,      {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_GT_DECIMAL,  {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_LT_INT,      {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_LT_DECIMAL,  {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_GTE_INT,     {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_GTE_DECIMAL, {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_LTE_INT,     {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CMP_LTE_DECIMAL, {COMPARISON,     INDEX,       INDEX,   INDEX}},
            {CAST_DECIMAL,    {OTHER,          INDEX,       UNUSED,  INDEX}},
            {MOD_DECIMAL,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {MOD_INT,         {OTHER,          INDEX,       INDEX,   INDEX}},
            {MUL_DECIMAL,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {MUL_INT,         {OTHER,          INDEX,       INDEX,   INDEX}},
            {DIV_DECIMAL,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {DIV_INT,         {OTHER,          INDEX,       INDEX,   INDEX}},
            {SUB_DECIMAL,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {SUB_INT,         {OTHER,          INDEX,       INDEX,   INDEX}},
            {ADD_DECIMAL,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {ADD_STRING,      {OTHER,          INDEX,       INDEX,   INDEX}},
            {ADD_INT,         {OTHER,          INDEX,       INDEX,   INDEX}},
            {CALL_NATIVE,     {OTHER,          INDEX,       INDEX,   INDEX}},
            {CALL,            {OTHER,          IMM_INT,     IMM_INT, IMM_INT}},
            {MOV,             {OTHER,          INDEX,       UNUSED,  INDEX}},
            {MOV_FNC,         {OTHER,          IMM_ADDRESS, UNUSED,  INDEX}},
            {MOV_NULL,        {OTHER,          UNUSED,      UNUSED,  INDEX}},
            {MOV_INT,         {OTHER,          IMM_INT,     UNUSED,  INDEX}},
            {MOV_BOOLEAN,     {OTHER,          IMM_INT,     UNUSED,  INDEX}},
            {MOV_DECIMAL,     {OTHER,          IMM_DECIMAL, UNUSED,  INDEX}},
            {MOV_STRING,      {OTHER,          IMM_STRING,  UNUSED,  INDEX}},
            {JMP_TRUE,        {JUMP,           INDEX,       UNUSED,  IMM_ADDRESS}},
            {JMP_FALSE,       {JUMP,           INDEX,       UNUSED,  IMM_ADDRESS}},
            {JMP,             {JUMP,           UNUSED,      UNUSED,  IMM_ADDRESS}},
            {FN_ENTER_STACK,  {FUNCTION_ENTER, IMM_INT,     UNUSED,  UNUSED}},
            {FN_ENTER_HEAP,   {FUNCTION_ENTER, IMM_INT,     UNUSED,  UNUSED}}
    };

    class Instruction {
    public:

        class Impl;

        uint64_t opCode = 0;
        union {
            uint64_t operand1 = 0;
            string *operand1AsLabel;
            double operand1AsDecimal;
        };
        union {
            uint64_t operand2 = 0;
        };
        union {
            string *destinationAsLabel;
            uint64_t destination = 0;
        };
        string comment;

        Instruction();

        Instruction *withOpCode(unsigned int opCode);

        Instruction *withOp1(unsigned int op);

        Instruction *withOp1(string *label);

        Instruction *withOp1(float decimal);

        Instruction *withOp2(unsigned int op);

        Instruction *withDestination(unsigned int dest);

        Instruction *withDestination(string *dest);

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

        vector<Instruction *> getInstructions();

    private:
        Impl *impl;
    };
}