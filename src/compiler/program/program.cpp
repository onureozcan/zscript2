#include <common/program.h>

#include <vector>

using namespace std;

namespace zero {

    class Program::Impl {
    private:
        string fileName;
        vector<pair<string, Instruction>> instructions;

    public:
        Impl(string fileName) {
            this->fileName = fileName;
        }

        void addInstruction(Instruction instruction, string label = "") {
            instructions.push_back({label, instruction});
        }

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
                case FN_ENTER:
                    return "FN_ENTER";
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
                case CALL:
                    return "CALL";
                case CMP:
                    return "CMP";
                case CAST_F:
                    return "CAST_F";
                default:
                    return "";
            };
        }

        string toString() {
            string instructionCode;
            for (const auto &labelInsPair: instructions) {

                auto ins = labelInsPair.second;
                auto label = labelInsPair.first;

                if (ins.opCode == LABEL) {
                    instructionCode += label + ":\n";
                    continue;
                }

                auto op1Str = to_string(ins.operand1);
                auto op2Str = to_string(ins.operand2);
                auto destinationStr = to_string(ins.destination);
                auto opcodeStr = opCodeToString(ins.opCode);
                auto opTypeStr = opTypeToString(ins.opType);

                if (ins.opType == FNC) {
                    op1Str = *(string *) ins.operand1AsPtr;
                }
                instructionCode +=
                        "\t" + opcodeStr + opTypeStr + ", " + op1Str + ", " + op2Str + ", " + destinationStr + "\t#" +
                        ins.comment + "\n";
            }
            return instructionCode;
        }

        void merge(Program *other) {
            for (auto &labelInsPair: other->impl->instructions) {
                this->instructions.push_back(labelInsPair);
            }
        }

        void addInstructionAt(Instruction instruction, string labelToFind) {
            int i = 0;
            for (const auto &labelInsPair: instructions) {
                i++;
                auto label = labelInsPair.first;
                if (label == labelToFind) {
                    this->instructions.insert(this->instructions.begin() + i, {"", instruction});
                    break;
                }
            }
        }
    };

    Program::Program(string fileName) {
        this->impl = new Impl(fileName);
    }

    void Program::addInstruction(Instruction instruction) {
        this->impl->addInstruction(instruction);
    }

    string Program::toString() {
        return impl->toString();
    }

    void Program::addLabel(string label) {
        return impl->addInstruction({LABEL}, label);
    }

    void Program::merge(Program *other) {
        impl->merge(other);
    }

    void Program::addInstructionAt(Instruction instruction, string label) {
        impl->addInstructionAt(instruction, label);
    }
}