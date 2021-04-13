#include <common/program.h>

#include <vector>

using namespace std;

namespace zero {

    class Instruction::Impl {
    public:
        static string opTypeToString(unsigned int opType) {
            switch ((OpType) opType) {
                case DECIMAL:
                    return "_DECIMAL";
                case FNC:
                    return "_FNC";
                case INT:
                    return "_INT";
                case NATIVE:
                    return "_NATIVE";
                case ANY:
                    return "_ANY";
                case STRING:
                    return "_STRING";
                default:
                    return "";
            };
        }

        static string opCodeToString(unsigned int opcode) {
            switch ((Opcode) opcode) {
                case FN_ENTER_HEAP:
                    return "FN_ENTER_HEAP";
                case FN_ENTER_STACK:
                    return "FN_ENTER_STACK";
                case JMP:
                    return "JMP";
                case JMP_EQ:
                    return "JMP_EQ";
                case JMP_NEQ:
                    return "JMP_NEQ";
                case JMP_GT:
                    return "JMP_GT";
                case JMP_GTE:
                    return "JMP_GTE";
                case JMP_LT:
                    return "JMP_LT";
                case JMP_LTE:
                    return "JMP_LTE";
                case MOV:
                    return "MOV";
                case MOV_I:
                    return "MOV_I";
                case ADD:
                    return "ADD";
                case DIV:
                    return "DIV";
                case SUB:
                    return "SUB";
                case MOD:
                    return "MOD";
                case RET:
                    return "RET";
                case MUL:
                    return "MUL";
                case CMP_EQ:
                    return "CMP_EQ";
                case CMP_NEQ:
                    return "CMP_NEQ";
                case CMP_GT:
                    return "CMP_GT";
                case CMP_GTE:
                    return "CMP_GTE";
                case CMP_LT:
                    return "CMP_LT";
                case CMP_LTE:
                    return "CMP_LTE";
                case CALL:
                    return "CALL";
                case CAST_F:
                    return "CAST_F";
                case NEG:
                    return "NEG";
                default:
                    return "";
            };
        }
    };

    class Program::Impl {
    private:
        string fileName;
        vector<Instruction *> instructions;

    public:
        Impl(string fileName) {
            this->fileName = fileName;
        }

        void addInstruction(Instruction *instruction, string label = "") {
            instructions.push_back(instruction);
        }

        string toString() {
            string instructionCode;
            for (const auto &ins: instructions) {
                instructionCode += ins->toString();
            }
            return "program of file at `" + fileName + "`:\n" + instructionCode;
        }

        void merge(Program *other) {
            for (auto &labelInsPair: other->impl->instructions) {
                this->instructions.push_back(labelInsPair);
            }
        }

        void addInstructionAt(Instruction *instruction, string labelToFind) {
            int i = 0;
            for (const auto &ins: instructions) {
                i++;
                auto label = ins->operand1AsLabel;
                if (ins->opCode == LABEL && label != nullptr && *label == labelToFind) {
                    this->instructions.insert(this->instructions.begin() + i, instruction);
                    break;
                }
            }
        }
    };

    Program::Program(string fileName) {
        this->impl = new Impl(fileName);
    }

    void Program::addInstruction(Instruction *instruction) {
        this->impl->addInstruction(instruction);
    }

    string Program::toString() {
        return impl->toString();
    }

    void Program::addLabel(string *label) {
        return impl->addInstruction((new Instruction())->withOpCode(LABEL)->withOp1(label));
    }

    void Program::merge(Program *other) {
        impl->merge(other);
    }

    void Program::addInstructionAt(Instruction *instruction, string label) {
        impl->addInstructionAt(instruction, label);
    }

    Instruction *Instruction::withOpCode(unsigned short opCode) {
        this->opCode = opCode;
        return this;
    }

    Instruction *Instruction::withOpType(unsigned short type) {
        this->opType = type;
        return this;
    }

    Instruction *Instruction::withOp1(unsigned int op1) {
        this->operand1 = op1;
        return this;
    }

    Instruction *Instruction::withOp1(string *label) {
        this->operand1AsLabel = label;
        return this;
    }

    Instruction *Instruction::withOp1(float decimal) {
        this->operand1AsDecimal = decimal;
        return this;
    }

    Instruction *Instruction::withOp2(unsigned int op) {
        this->operand2 = op;
        return this;
    }

    Instruction *Instruction::withDestination(unsigned int dest) {
        this->destination = dest;
        return this;
    }

    Instruction *Instruction::withComment(string comment) {
        this->comment = comment;
        return this;
    }

    string Instruction::toString() const {
        if (opCode == LABEL) {
            return *operand1AsLabel + ":\n";
        }

        auto op1Str = to_string(operand1);
        auto op2Str = to_string(operand2);
        auto destinationStr = to_string(destination);
        auto opcodeStr = Instruction::Impl::opCodeToString(opCode);
        auto opTypeStr = Instruction::Impl::opTypeToString(opType);

        if (opType == FNC || opType == STRING) {
            op1Str = *operand1AsLabel;
        } else if (opType == DECIMAL) {
            op1Str = to_string(operand1AsDecimal);
        }
        return "\t" + opcodeStr + opTypeStr + ", " + op1Str + ", " + op2Str + ", " + destinationStr + "\t# " + comment +
               "\n";
    }

    Instruction::Instruction() {
        this->impl = new Impl();
    }
}