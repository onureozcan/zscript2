#include <common/program.h>

#include <vector>
#include <map>
#include <cstring>

using namespace std;

namespace zero {

    class Instruction::Impl {
    public:

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
                case JMP_GT_INT:
                    return "JMP_GT";
                case JMP_GT_DECIMAL:
                    return "JMP_GT_DECIMAL";
                case JMP_GTE_INT:
                    return "JMP_GTE_INT";
                case JMP_GTE_DECIMAL:
                    return "JMP_GTE_DECIMAL";
                case JMP_LT_INT:
                    return "JMP_LT_INT";
                case JMP_LT_DECIMAL:
                    return "JMP_LT_DECIMAL";
                case JMP_LTE_INT:
                    return "JMP_LTE";
                case JMP_LTE_DECIMAL:
                    return "JMP_LTE_DECIMAL";
                case MOV:
                    return "MOV";
                case MOV_INT:
                    return "MOV_INT";
                case MOV_DECIMAL:
                    return "MOV_DECIMAL";
                case MOV_STRING:
                    return "MOV_STRING";
                case MOV_FNC:
                    return "MOV_FNC";
                case ADD_INT:
                    return "ADD_INT";
                case ADD_STRING:
                    return "ADD_STRING";
                case ADD_DECIMAL:
                    return "ADD_DECIMAL";
                case DIV_INT:
                    return "DIV_INT";
                case DIV_DECIMAL:
                    return "DIV_DECIMAL";
                case SUB_INT:
                    return "SUB_INT";
                case SUB_DECIMAL:
                    return "SUB_DECIMAL";
                case MOD_INT:
                    return "MOD_INT";
                case MOD_DECIMAL:
                    return "MOD_DECIMAL";
                case RET:
                    return "RET";
                case MUL_INT:
                    return "MUL_INT";
                case MUL_DECIMAL:
                    return "MUL_DECIMAL";
                case CMP_EQ:
                    return "CMP_EQ";
                case CMP_NEQ:
                    return "CMP_NEQ";
                case CMP_GT_INT:
                    return "CMP_GT_INT";
                case CMP_GT_DECIMAL:
                    return "CMP_GT_DECIMAL";
                case CMP_GTE_INT:
                    return "CMP_GTE_INT";
                case CMP_GTE_DECIMAL:
                    return "CMP_GTE_DECIMAL";
                case CMP_LT_INT:
                    return "CMP_LT_INT";
                case CMP_LT_DECIMAL:
                    return "CMP_LT_DECIMAL";
                case CMP_LTE_INT:
                    return "CMP_LTE_INT";
                case CMP_LTE_DECIMAL:
                    return "CMP_LTE_DECIMAL";
                case CALL:
                    return "CALL";
                case CALL_NATIVE:
                    return "CALL_NATIVE";
                case CAST_DECIMAL:
                    return "CAST_DECIMAL";
                case NEG_INT:
                    return "NEG_INT";
                case NEG_DECIMAL:
                    return "NEG_DECIMAL";
                case PUSH:
                    return "PUSH";
                case POP:
                    return "POP";
                case ARG_READ:
                    return "ARG_READ";
                case GET_IN_PARENT:
                    return "GET_IN_PARENT";
                case SET_IN_PARENT:
                    return "SET_IN_PARENT";
                case GET_IN_OBJECT:
                    return "GET_IN_OBJECT";
                case SET_IN_OBJECT:
                    return "SET_IN_OBJECT";
                default:
                    return "";
            };
        }
    };

    class Program::Impl {
    private:
        string fileName;
        vector<Instruction *> instructions;
        vector<uint64_t> data;

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

        char *toBytes() {
            map<string *, uint64_t> labelPositions;

            int i = 0;
            for (auto &ins: instructions) {
                if (ins->opCode == LABEL) {
                    labelPositions[ins->operand1AsLabel] = i;
                } else {
                    i++;
                }
            }

            data.push_back(i); // write total instruction count

            for (auto &ins: instructions) {
                if (ins->opCode == LABEL) continue;
                data.push_back(ins->opCode);

                if (ins->opCode == MOV_FNC) {
                    auto labelIndex = labelPositions[ins->operand1AsLabel];
                    data.push_back(labelIndex);
                } else if (ins->opCode == MOV_STRING) {
                    auto cpy = new string(*ins->operand1AsLabel);
                    data.push_back((uint64_t) cpy);
                } else if (ins->opCode == MOV_DECIMAL) {
                    double double_value = ins->operand1AsDecimal;
                    char buf[sizeof(double)];
                    memcpy(buf, &double_value, sizeof(double));
                    uint64_t data_to_be_written = *(uint64_t *) buf;
                    data.push_back(data_to_be_written);
                } else {
                    data.push_back(ins->operand1);
                }

                data.push_back(ins->operand2);
                data.push_back(ins->destination);

            }

            return reinterpret_cast<char *>(data.data());
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

    char *Program::toBytes() {
        return impl->toBytes();
    }

    Instruction *Instruction::withOpCode(unsigned int opCode) {
        this->opCode = opCode;
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

        if (opCode == MOV_FNC || opCode == MOV_STRING) {
            op1Str = *operand1AsLabel;
        } else if (opCode == MOV_DECIMAL) {
            op1Str = to_string(operand1AsDecimal);
        }
        return "\t" + opcodeStr + ", " + op1Str + ", " + op2Str + ", " + destinationStr + "\t# " + comment +
               "\n";
    }

    Instruction::Instruction() {
        this->impl = new Impl();
    }
}