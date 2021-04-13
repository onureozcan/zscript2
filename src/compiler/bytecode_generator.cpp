#include <common/program.h>
#include <compiler/compiler.h>
#include <common/logger.h>
#include <compiler/op.h>

using namespace std;

namespace zero {

    class TempVariableAllocator {
    private:
        map<int, int> tempVariableOccupancyMap; // temp variable index - occupied map
        TypeInfo *contextObj;
    public:

        explicit TempVariableAllocator(TypeInfo *contextObj) {
            this->contextObj = contextObj;
        }

        unsigned int tempCount() {
            return tempVariableOccupancyMap.size();
        }

        unsigned int alloc() {
            unsigned int freeIndex = 0;
            for (auto &entry: tempVariableOccupancyMap) {
                if (entry.second == 0) {
                    freeIndex = entry.first;
                    break;
                }
            }
            if (freeIndex != 0) {
                tempVariableOccupancyMap[freeIndex] = 1;
                return freeIndex;
            } else {
                // alloc new
                unsigned int index = contextObj->addProperty("$temp_" + to_string(tempVariableOccupancyMap.size()),
                                                             &TypeInfo::ANY);
                tempVariableOccupancyMap[index] = 1;
                return index;
            }
        }

        unsigned int release(unsigned int variableIndex) {
            return tempVariableOccupancyMap[variableIndex] = 0;
        }
    };

    static string *programEntryLabel = new string(" .programEntry");

    class ByteCodeGenerator::Impl {

    public:
        TypeMetadataRepository *typeMetadataRepository = nullptr;

        Program *generate(ProgramAstNode *programAstNode) {
            return doGenerateCode(programAstNode);
        }

    private:
        Logger log = Logger("bytecode_generator");

        Program *rootProgram = nullptr;
        vector<Program *> subroutinePrograms; // list of subroutines created so far

        vector<Program *> programsStack; // to keep track of current program
        vector<FunctionAstNode *> functionAstStack; // to keep track of current function ast

        map<string, TempVariableAllocator *> tempVariableAllocatorMap;

        Program *currentProgram() {
            if (programsStack.empty()) return nullptr;
            return programsStack.back();
        }

        FunctionAstNode *currentFunctionAst() {
            if (functionAstStack.empty()) return nullptr;
            return functionAstStack.back();
        }

        TempVariableAllocator *currentTempVariableAllocator() {
            auto currentContextType = currentFunctionAst()->program->contextObjectTypeName;
            return tempVariableAllocatorMap[currentContextType];
        }

        TypeInfo *type(string name) {
            return typeMetadataRepository->findTypeByName(name);
        }

        Operator *getOp(string name, int operandCount) {
            return Operator::getBy(name, operandCount);
        }

        Program *doGenerateCode(ProgramAstNode *programAstNode) {
            auto globalFnc = new FunctionAstNode();
            globalFnc->program = programAstNode;
            globalFnc->arguments = new vector<pair<string, string>>();
            globalFnc->fileName = programAstNode->fileName;
            globalFnc->line = 0;
            globalFnc->pos = 0;

            visitFunction(globalFnc);

            rootProgram = new Program(programAstNode->fileName);
            for (auto &sub: subroutinePrograms) {
                rootProgram->merge(sub);
            }

            return rootProgram;
        }


        Program *onFunctionEnter(FunctionAstNode *functionAstNode, string *label) {
            auto parentFunction = currentFunctionAst();
            if (parentFunction != nullptr) {
                // the parent does have a child, we (unfortunately) have to alloc its temporary variables from the heap!
                parentFunction->isLeafFunction = false;
            }

            auto sub = new Program(functionAstNode->fileName);
            sub->addLabel(label);
            sub->addLabel(programEntryLabel);
            subroutinePrograms.push_back(sub);
            programsStack.push_back(sub);
            functionAstStack.push_back(functionAstNode);
            functionAstNode->isLeafFunction = true; // set it as leaf for now

            tempVariableAllocatorMap[functionAstNode->program->contextObjectTypeName] =
                    (new TempVariableAllocator(type(functionAstNode->program->contextObjectTypeName)));

            return sub;
        }

        unsigned int visitFunction(FunctionAstNode *function,
                                   unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            // -- entry
            auto *fnLabel = new string("fun@" + to_string(function->line) + "_" + to_string(function->pos));

            auto parentProgram = currentProgram();
            if (parentProgram != nullptr) { // global
                // let the parent know about our address so that it can call us
                parentProgram->addInstruction(
                        (new Instruction())->withOpCode(MOV)
                                ->withOpType(FNC)
                                ->withOp1(fnLabel)
                                ->withDestination(preferredIndex)
                                ->withComment("mov function address to index " + to_string(preferredIndex) +
                                              " in the current frame")
                );
            }
            onFunctionEnter(function, fnLabel);

            TypeInfo *contextObjectType = type(currentFunctionAst()->program->contextObjectTypeName);

            // --- function body

            for (auto &stmt:*function->program->statements) {
                visitStatement(stmt);
            }

            // ----- exit

            unsigned int functionContextObjectSize = contextObjectType->getPropertyCount();

            currentProgram()->addInstructionAt(
                    (new Instruction())->withOpCode(
                                    currentFunctionAst()->isLeafFunction ? FN_ENTER_STACK : FN_ENTER_HEAP)
                            ->withOp1(functionContextObjectSize)
                            ->withComment("allocate call frame that is " + to_string(functionContextObjectSize) +
                                          " values big"),
                    *programEntryLabel);

            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(RET)
                            ->withDestination(0)
                            ->withComment("redundant null-return for non-returning functions "));

            programsStack.pop_back();
            functionAstStack.pop_back();

            return preferredIndex;
        }

        unsigned int visitAtom(AtomicExpressionAstNode *atomic,
                               unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOpType(DECIMAL)
                                    ->withOp1((float) atof(atomic->data.c_str()))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load decimal into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_INT: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOpType(INT)
                                    ->withOp1((unsigned) (stoi(atomic->data)))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load int into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_STRING: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOpType(STRING)
                                    ->withOp1(&(atomic->data))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load string into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_FUNCTION: {
                    return visitFunction((FunctionAstNode *) atomic, preferredIndex);
                }
                case AtomicExpressionAstNode::TYPE_IDENTIFIER: {
                    break;
                }
            }
            return atomic->memoryIndex;
        }

        unsigned int visitFunctionCall(FunctionCallExpressionAstNode *function) {
            return 0;
        }

        unsigned int visitBinary(BinaryExpressionAstNode *binary,
                                 unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            Operator *op = getOp(binary->opName, 2);
            if (op == &Operator::DOT) {
                //visitDot(binary);
            } else if (op == &Operator::ASSIGN) {
                //visitAssign(binary);
            } else {
                unsigned int valueIndex1 = currentTempVariableAllocator()->alloc();
                unsigned int valueIndex2 = currentTempVariableAllocator()->alloc();
                unsigned int actualValueIndex1 = visitExpression(binary->left, valueIndex1);
                unsigned int actualValueIndex2 = visitExpression(binary->right, valueIndex2);

                unsigned short opCode = 0;
                if (op == &Operator::ADD) {
                    opCode = Opcode::ADD;
                } else if (op == &Operator::SUB) {
                    opCode = Opcode::SUB;
                } else if (op == &Operator::DIV) {
                    opCode = Opcode::DIV;
                } else if (op == &Operator::MUL) {
                    opCode = Opcode::MUL;
                } else if (op == &Operator::MOD) {
                    opCode = Opcode::MOD;
                } else if (op == &Operator::CMP_E) {
                    opCode = Opcode::CMP_EQ;
                } else if (op == &Operator::CMP_NE) {
                    opCode = Opcode::CMP_NEQ;
                } else if (op == &Operator::GT) {
                    opCode = Opcode::CMP_GT;
                } else if (op == &Operator::GTE) {
                    opCode = Opcode::CMP_GTE;
                } else if (op == &Operator::LT) {
                    opCode = Opcode::CMP_LT;
                } else if (op == &Operator::LTE) {
                    opCode = Opcode::CMP_LTE;
                }
                auto typeOfBinary = type(binary->typeName);

                unsigned short opType = OpType::INT;
                if (typeOfBinary == &TypeInfo::DECIMAL) {
                    opType = DECIMAL;
                } else if (typeOfBinary == &TypeInfo::STRING) {
                    opType = STRING;
                }
                currentProgram()->addInstruction(
                        (new Instruction())
                                ->withOpCode(opCode)
                                ->withOpType(opType)
                                ->withOp1(actualValueIndex1)
                                ->withOp2(actualValueIndex2)
                                ->withDestination(preferredIndex)
                                ->withComment(
                                        op->name + " 2 values at indexes " + to_string(actualValueIndex1) + " and " +
                                        to_string(actualValueIndex2) + " into " + to_string(preferredIndex))
                );

                if (actualValueIndex1 == valueIndex1) {
                    currentTempVariableAllocator()->release(actualValueIndex1);
                }

                if (actualValueIndex2 == valueIndex2) {
                    currentTempVariableAllocator()->release(actualValueIndex2);
                }

                return preferredIndex;
            }

            return 0;
        }

        unsigned int visitPrefix(PrefixExpressionAstNode *prefix,
                                 unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            unsigned int actualValueIndex = visitExpression(prefix->right, preferredIndex);
            auto op = getOp(prefix->opName, 1);

            unsigned short opCode = 0;
            if (op == &Operator::NEG) {
                opCode = Opcode::NEG;
            } else {
                // TODO: implement other operators also
            }

            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(opCode)
                            ->withOp1(actualValueIndex)
                            ->withDestination(preferredIndex)
                            ->withComment(
                                    "took " + op->name + " of value at index " + to_string(actualValueIndex) +
                                    "and put it into " + to_string(preferredIndex) + " in the current frame")
            );

            return preferredIndex;
        }

        unsigned int visitExpression(ExpressionAstNode *expression,
                                     unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            switch (expression->expressionType) {
                case ExpressionAstNode::TYPE_ATOMIC : {
                    return visitAtom((AtomicExpressionAstNode *) expression, preferredIndex);
                }
                case ExpressionAstNode::TYPE_BINARY: {
                    return visitBinary((BinaryExpressionAstNode *) expression, preferredIndex);
                }
                case ExpressionAstNode::TYPE_UNARY: {
                    return visitPrefix((PrefixExpressionAstNode *) expression, preferredIndex);
                }
                case ExpressionAstNode::TYPE_FUNCTION_CALL: {
                    //return visitFunctionCall((FunctionCallExpressionAstNode *) expression);
                    break;
                }
            }
            return expression->memoryIndex;
        }

        unsigned int visitReturn(StatementAstNode *stmt) {
            unsigned int valueIndex = 0;
            unsigned int actualValueIndex = 0;
            if (stmt->expression != nullptr) {
                valueIndex = currentTempVariableAllocator()->alloc();
                actualValueIndex = visitExpression(stmt->expression, valueIndex);
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(RET)
                            ->withDestination(valueIndex)
                            ->withComment("return value at " + to_string(actualValueIndex))
            );
            return valueIndex;
        }

        void cast(unsigned int valueIndex, TypeInfo *t1, TypeInfo *t2) {
            if (t1 != t2) {
                if (t1 == &TypeInfo::INT && t2 == &TypeInfo::DECIMAL) {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(CAST_F)
                                    ->withOp1(valueIndex)
                                    ->withDestination(valueIndex)
                                    ->withComment("auto cast fromm int to float")
                    );
                }
            }
        }

        unsigned short opType(string typeName) {
            OpType ret;
            auto typeObj = type(typeName);
            if (typeObj == &TypeInfo::DECIMAL) {
                ret = OpType::DECIMAL;
            } else if (typeObj == &TypeInfo::INT) {
                ret = OpType::INT;
            } else if (typeObj == &TypeInfo::STRING) {
                ret = OpType::STRING;
            } else if (typeObj->isNative) {
                ret = OpType::NATIVE;
            } else if (typeObj->isCallable) {
                ret = OpType::FNC;
            } else {
                ret = OpType::ANY;
            }
            return (unsigned short) ret;
        }

        void visitVariable(VariableAstNode *variable) {
            auto destinationIndex = variable->memoryIndex;
            auto expectedType = type(variable->typeName);

            if (variable->initialValue != nullptr) {
                unsigned int requestedValueIndex = destinationIndex;
                unsigned int actualValueIndex = visitExpression(variable->initialValue, requestedValueIndex);

                auto actualType = type(variable->initialValue->typeName);
                cast(actualValueIndex, actualType, expectedType);

                if (requestedValueIndex != actualValueIndex) {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOp1(actualValueIndex)
                                    ->withDestination(destinationIndex)
                                    ->withComment("mov value at index " + to_string(actualValueIndex) + " into index " +
                                                  to_string(destinationIndex) + " (" + variable->identifier + ")")
                    );
                }

            } else {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_I)
                                ->withOpType(opType(variable->typeName))
                                ->withOp1(variable->initialValue->memoryIndex)
                                ->withDestination(destinationIndex)
                                ->withComment("mov immediate value into index " +
                                              to_string(destinationIndex) + " (" + variable->identifier + ")")
                );
            }
        }

        void visitStatement(StatementAstNode *stmt) {
            if (stmt->type == StatementAstNode::TYPE_EXPRESSION) {
                visitExpression(stmt->expression);
            } else if (stmt->type == StatementAstNode::TYPE_RETURN) {
                visitReturn(stmt);
            } else {
                visitVariable(stmt->variable);
            }
        }
    };

    Program *ByteCodeGenerator::generate(ProgramAstNode *programAstNode) {
        return impl->generate(programAstNode);
    }

    ByteCodeGenerator::ByteCodeGenerator(TypeMetadataRepository *typeMetadataRepository) {
        this->impl = new ByteCodeGenerator::Impl();
        this->impl->typeMetadataRepository = typeMetadataRepository;
    }
}