#include <common/program.h>
#include <compiler/compiler.h>
#include <common/logger.h>
#include <compiler/op.h>

#include <map>

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
                                   unsigned int preferredIndex = 0
        ) {
            // -- entry
            auto *fnLabel = new string("fun@" + to_string(function->line) + "_" + to_string(function->pos));

            auto parentProgram = currentProgram();
            if (parentProgram != nullptr) { // global
                // let the parent know about our address so that it can call us
                parentProgram->addInstruction(
                        (new Instruction())->withOpCode(MOV_FNC)
                                ->withOp1(fnLabel)
                                ->withDestination(preferredIndex)
                                ->withComment("mov function address to index " + to_string(preferredIndex) +
                                              " in the current frame")
                );
            }
            onFunctionEnter(function, fnLabel);

            TypeInfo *contextObjectType = type(currentFunctionAst()->program->contextObjectTypeName);

            // --- function body

            for (int i = 0; i < function->arguments->size(); i++) {
                auto argPair = function->arguments->at(i);
                auto argIndex = contextObjectType->getProperty(argPair.first)->index;
                currentProgram()->addInstruction(
                        (new Instruction())
                                ->withOpCode(ARG_READ)
                                ->withOp1((unsigned int) (function->arguments->size() - i - 1))
                                ->withDestination(argIndex)
                                ->withComment(
                                        "getting argument at index " + to_string(i) + " into " + to_string(argIndex))
                );
            }

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
                            ->withDestination((unsigned) 0)
                            ->withComment("redundant null-return for non-returning functions "));

            programsStack.pop_back();
            functionAstStack.pop_back();

            return preferredIndex;
        }

        unsigned int visitAtom(AtomicExpressionAstNode *atomic,
                               unsigned int preferredIndex = 0
        ) {
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV_DECIMAL)
                                    ->withOp1((float) atof(atomic->data.c_str()))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load decimal into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_INT: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV_INT)
                                    ->withOp1((unsigned) (stoi(atomic->data)))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load int into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_BOOLEAN: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV_BOOLEAN)
                                    ->withOp1((unsigned) (atomic->data == "true" ? 1 : 0))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load boolean into index " + to_string(preferredIndex) +
                                                  " in the current frame - " + atomic->toString())
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_STRING: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV_STRING)
                                    ->withOp1(&(atomic->data))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load string into index " + to_string(preferredIndex) +
                                                  " in the current frame ")
                    );
                    return preferredIndex;
                }
                case AtomicExpressionAstNode::TYPE_FUNCTION: {
                    return visitFunction((FunctionAstNode *) atomic, preferredIndex);
                }
                case AtomicExpressionAstNode::TYPE_IDENTIFIER: {
                    if (atomic->memoryDepth == 0) {
                        // in the current frame, simple, say its address relative to current frame
                        return atomic->memoryIndex;
                    } else {
                        // in a parent frame
                        currentProgram()->addInstruction(
                                (new Instruction())
                                        ->withOpCode(GET_IN_PARENT)
                                        ->withOp1(atomic->memoryDepth)
                                        ->withOp2(atomic->memoryIndex)
                                        ->withDestination(preferredIndex)
                                        ->withComment(
                                                "getting the value at index " + to_string(atomic->memoryIndex)
                                                + " at parent with depth " + to_string(atomic->memoryDepth) +
                                                " into index " + to_string(preferredIndex) + " in the current frame (" +
                                                atomic->data + ")"
                                        )
                        );
                        return preferredIndex;
                    }
                }
            }
            return atomic->memoryIndex;
        }

        unsigned int visitFunctionCall(FunctionCallExpressionAstNode *functionCall,
                                       unsigned int preferredIndex
        ) {
            unsigned int tempIndex = currentTempVariableAllocator()->alloc();
            for (unsigned int i = 0; i < functionCall->params->size(); i++) {
                ExpressionAstNode *param = functionCall->params->at(i);
                unsigned int paramValueIndex = visitExpression(param, tempIndex);
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(PUSH)
                                ->withOp1(paramValueIndex)
                                ->withComment("pushing param number " + to_string(i) + " which is at index " +
                                              to_string(paramValueIndex))
                );
            }
            currentTempVariableAllocator()->release(tempIndex);
            auto functionType = type(functionCall->left->typeName);
            auto opCode = functionType->isNative ? CALL_NATIVE : CALL;

            unsigned int functionIndex = visitExpression(functionCall->left, preferredIndex);
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(opCode)
                            ->withOp1(functionIndex)
                            ->withDestination(preferredIndex)
                            ->withComment("calling functionCall at index " + to_string(functionIndex))
            );

            return preferredIndex;
        }

        void visitAssignment(BinaryExpressionAstNode *binary, unsigned int preferredIndex) {

            unsigned int valueIndex = visitExpression(binary->right, preferredIndex);

            if (binary->left->expressionType == ExpressionAstNode::TYPE_ATOMIC) {
                // assign without DOT operation
                unsigned int memoryDepth = binary->left->memoryDepth;
                unsigned int memoryIndex = binary->left->memoryIndex;

                if (memoryDepth == 0) {
                    // set in current context
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOp1(valueIndex)
                                    ->withDestination(memoryIndex)
                                    ->withComment("mov value at index " + to_string(valueIndex) + " into index " +
                                                  to_string(memoryIndex) + " in the current frame")
                    );
                } else {
                    // set in parent context
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(SET_IN_PARENT)
                                    ->withOp1(memoryDepth)
                                    ->withOp2(valueIndex)
                                    ->withDestination(memoryIndex)
                                    ->withComment("set value at parent index with depth " +
                                                  to_string(memoryDepth) + " and index " + to_string(memoryIndex) +
                                                  " from index " + to_string(valueIndex) + " in the current frame")
                    );
                }
            } else {
                // DOT assign
                // TODO
            }
        }

        unsigned int visitAnd(BinaryExpressionAstNode *binary,
                              unsigned int preferredIndex = 0
        ) {
            auto *falseLabel = new string("__and_false_" + to_string(binary->line) + "_" + to_string(binary->pos));
            auto *trueLabel = new string("__and_true_" + to_string(binary->line) + "_" + to_string(binary->pos));
            auto *endLabel = new string("__and_end_" + to_string(binary->line) + "_" + to_string(binary->pos));

            unsigned int actualValueIndex1 = visitExpression(binary->left, preferredIndex);
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP_FALSE)
                            ->withOp1(actualValueIndex1)
                            ->withDestination(falseLabel)
                            ->withComment("short  circuit and jmp if v1 is false")
            );
            unsigned int actualValueIndex2 = visitExpression(binary->right, preferredIndex);
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP_TRUE)
                            ->withOp1(actualValueIndex2)
                            ->withDestination(trueLabel)
                            ->withComment("short circuit and jmp if v2 is true")
            );
            currentProgram()->addLabel(falseLabel);
            if (actualValueIndex1 != preferredIndex) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_BOOLEAN)
                                ->withOp1((unsigned) 0)
                                ->withDestination(preferredIndex)
                                ->withComment("short circuit and false branch")
                );
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP)
                            ->withDestination(endLabel)
                            ->withComment("short circuit and false jmp to end")
            );
            currentProgram()->addLabel(trueLabel);
            if (actualValueIndex2 != preferredIndex) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_BOOLEAN)
                                ->withOp1((unsigned) 1)
                                ->withDestination(preferredIndex)
                                ->withComment("short circuit and true branch")
                );
            }
            currentProgram()->addLabel(endLabel);

            return preferredIndex;
        }

        unsigned int visitOr(BinaryExpressionAstNode *binary,
                             unsigned int preferredIndex = 0
        ) {
            auto *endLabel = new string("__or_end_" + to_string(binary->line) + "_" + to_string(binary->pos));

            unsigned int actualValueIndex1 = visitExpression(binary->left, preferredIndex);
            if (actualValueIndex1 != preferredIndex) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV)
                                ->withOp1(actualValueIndex1)
                                ->withDestination(preferredIndex)
                                ->withComment("short circuit and v1 branch, dest is " + to_string(preferredIndex))
                );
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP_TRUE)
                            ->withOp1(actualValueIndex1)
                            ->withDestination(endLabel)
                            ->withComment("short  circuit or jmp if v1 is true")
            );
            unsigned int actualValueIndex2 = visitExpression(binary->right, preferredIndex);
            if (actualValueIndex2 != preferredIndex) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV)
                                ->withOp1(actualValueIndex1)
                                ->withDestination(preferredIndex)
                                ->withComment("short circuit and v2 branch, dest is " + to_string(preferredIndex))
                );
            }
            currentProgram()->addLabel(endLabel);
            return preferredIndex;
        }

        unsigned int visitBinary(BinaryExpressionAstNode *binary,
                                 unsigned int preferredIndex = 0
        ) {
            Operator *op = getOp(binary->opName, 2);
            if (op == &Operator::DOT) {
                //visitDot(binary);
            } else if (op == &Operator::ASSIGN) {
                visitAssignment(binary, preferredIndex);
            } else if (op == &Operator::AND) {
                return visitAnd(binary, preferredIndex);
            } else if (op == &Operator::OR) {
                return visitOr(binary, preferredIndex);
            } else {
                unsigned int valueIndex1 = currentTempVariableAllocator()->alloc();
                unsigned int valueIndex2 = currentTempVariableAllocator()->alloc();
                unsigned int actualValueIndex1 = visitExpression(binary->left, valueIndex1);
                unsigned int actualValueIndex2 = visitExpression(binary->right, valueIndex2);

                unsigned short opCode = 0;
                auto typeOfBinary = type(binary->typeName);
                auto isDecimalOp = binary->left->typeName == TypeInfo::DECIMAL.name ||
                                   binary->right->typeName == TypeInfo::DECIMAL.name;

                if (isDecimalOp) {
                    // auto casting
                    if (binary->left->typeName == TypeInfo::INT.name) {
                        currentProgram()->addInstruction(
                                (new Instruction())->withOpCode(CAST_DECIMAL)
                                        ->withOp1(actualValueIndex1)
                                        ->withDestination(actualValueIndex1)
                                        ->withComment("auto cast from int to decimal")
                        );
                    }
                    if (binary->right->typeName == TypeInfo::INT.name) {
                        currentProgram()->addInstruction(
                                (new Instruction())->withOpCode(CAST_DECIMAL)
                                        ->withOp1(actualValueIndex2)
                                        ->withDestination(actualValueIndex2)
                                        ->withComment("auto cast from int to decimal")
                        );
                    }
                }

                if (op == &Operator::ADD) {
                    if (typeOfBinary == &TypeInfo::DECIMAL) {
                        opCode = ADD_DECIMAL;
                    } else if (typeOfBinary == &TypeInfo::STRING) {
                        opCode = ADD_STRING;
                    } else {
                        opCode = ADD_INT;
                    }
                } else if (op == &Operator::SUB) {
                    if (typeOfBinary == &TypeInfo::DECIMAL) {
                        opCode = SUB_DECIMAL;
                    } else {
                        opCode = SUB_INT;
                    }
                } else if (op == &Operator::DIV) {
                    if (typeOfBinary == &TypeInfo::DECIMAL) {
                        opCode = DIV_DECIMAL;
                    } else {
                        opCode = DIV_INT;
                    }
                } else if (op == &Operator::MUL) {
                    if (typeOfBinary == &TypeInfo::DECIMAL) {
                        opCode = MUL_DECIMAL;
                    } else {
                        opCode = MUL_INT;
                    }
                } else if (op == &Operator::MOD) {
                    if (typeOfBinary == &TypeInfo::DECIMAL) {
                        opCode = MOD_DECIMAL;
                    } else {
                        opCode = MOD_INT;
                    }
                } else if (op == &Operator::CMP_E) {
                    opCode = CMP_EQ;
                } else if (op == &Operator::CMP_NE) {
                    opCode = CMP_NEQ;
                } else if (op == &Operator::GT) {
                    if (isDecimalOp) {
                        opCode = CMP_GT_DECIMAL;
                    } else {
                        opCode = CMP_GT_INT;
                    }
                } else if (op == &Operator::GTE) {
                    if (isDecimalOp) {
                        opCode = CMP_GTE_DECIMAL;
                    } else {
                        opCode = CMP_GTE_INT;
                    }
                } else if (op == &Operator::LT) {
                    if (isDecimalOp) {
                        opCode = CMP_LT_DECIMAL;
                    } else {
                        opCode = CMP_LT_INT;
                    }
                } else if (op == &Operator::LTE) {
                    if (isDecimalOp) {
                        opCode = CMP_LTE_DECIMAL;
                    } else {
                        opCode = CMP_LTE_INT;
                    }
                }

                currentProgram()->addInstruction(
                        (new Instruction())
                                ->withOpCode(opCode)
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
                                 unsigned int preferredIndex = 0
        ) {
            unsigned int actualValueIndex = visitExpression(prefix->right, preferredIndex);
            auto op = getOp(prefix->opName, 1);

            unsigned short opCode = 0;
            if (op == &Operator::NEG) {
                if (prefix->typeName == TypeInfo::DECIMAL.name) {
                    opCode = Opcode::NEG_DECIMAL;
                } else {
                    opCode = Opcode::NEG_INT;
                }
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
                                     unsigned int preferredIndex = 0
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
                    return visitFunctionCall((FunctionCallExpressionAstNode *) expression, preferredIndex);
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
                if (actualValueIndex != valueIndex) {
                    currentTempVariableAllocator()->release(valueIndex);
                    valueIndex = actualValueIndex;
                }
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
                            (new Instruction())->withOpCode(CAST_DECIMAL)
                                    ->withOp1(valueIndex)
                                    ->withDestination(valueIndex)
                                    ->withComment("auto cast fromm int to float")
                    );
                }
            }
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
                auto typeObj = type(variable->typeName);
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(typeObj == &TypeInfo::DECIMAL ? MOV_DECIMAL : MOV_INT)
                                ->withOp1(variable->initialValue->memoryIndex)
                                ->withDestination(destinationIndex)
                                ->withComment("mov immediate value into index " +
                                              to_string(destinationIndex) + " (" + variable->identifier + ")")
                );
            }
        }

        void visitIfStatement(IfStatementAstNode *ifStatementAstNode) {
            auto ifFalseLabel = new string(
                    "__if_false__" + to_string(ifStatementAstNode->line) + "_" + to_string(ifStatementAstNode->pos));
            auto ifEndLabel = new string(
                    "__if_end__" + to_string(ifStatementAstNode->line) + "_" + to_string(ifStatementAstNode->pos));

            unsigned tempIndex = currentTempVariableAllocator()->alloc();
            unsigned int expressionValueIndex = visitExpression(ifStatementAstNode->expression, tempIndex);

            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP_FALSE)
                            ->withOp1(expressionValueIndex)
                            ->withDestination(ifFalseLabel)
                            ->withComment("if condition check")
            );
            currentTempVariableAllocator()->release(tempIndex);

            auto statements = ifStatementAstNode->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP)
                            ->withDestination(ifEndLabel)
                            ->withComment("if goto end")
            );
            currentProgram()->addLabel(ifFalseLabel);
            if (ifStatementAstNode->elseProgram != nullptr) {
                statements = ifStatementAstNode->elseProgram->statements;
                for (auto stmt: *statements) {
                    visitStatement(stmt);
                }
            }
            currentProgram()->addLabel(ifEndLabel);
        }

        void visitLoop(LoopAstNode *loop) {
            auto loopBodyLabel = new string("__loop_body__" + to_string(loop->line) + "_" + to_string(loop->pos));
            auto loopEndLabel = new string("__loop_end__" + to_string(loop->line) + "_" + to_string(loop->pos));
            auto loopConditionLabel = new string(
                    "__loop_condition__" + to_string(loop->line) + "_" + to_string(loop->pos));

            auto loopConditionTempIndex = currentTempVariableAllocator()->alloc();
            auto loopIterationResultTempIndex = currentTempVariableAllocator()->alloc();
            unsigned int actualLoopConditionIndex = 0;
            unsigned int actualLoopIterationResultIndex = 0;

            if (loop->loopVariable != nullptr) {
                visitVariable(loop->loopVariable);
            }

            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP)
                            ->withDestination(loopConditionLabel)
            );

            currentProgram()->addLabel(loopBodyLabel);
            auto statements = loop->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }

            if (loop->loopIterationExpression != nullptr) {
                actualLoopIterationResultIndex = visitExpression(loop->loopIterationExpression, loopIterationResultTempIndex);
            }

            currentProgram()->addLabel(loopConditionLabel);
            if (loop->loopConditionExpression != nullptr) {
                actualLoopConditionIndex = visitExpression(loop->loopConditionExpression, loopConditionTempIndex);
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(JMP_TRUE)
                            ->withOp1(actualLoopConditionIndex)
                            ->withDestination(loopBodyLabel)
            );

            currentProgram()->addLabel(loopEndLabel);
        }

        void visitStatement(StatementAstNode *stmt) {
            unsigned tempIndex = currentTempVariableAllocator()->alloc();
            if (stmt->type == StatementAstNode::TYPE_EXPRESSION) {
                visitExpression(stmt->expression, tempIndex);
            } else if (stmt->type == StatementAstNode::TYPE_RETURN) {
                visitReturn(stmt);
            } else if (stmt->type == StatementAstNode::TYPE_IF) {
                visitIfStatement(stmt->ifStatement);
            } else if (stmt->type == StatementAstNode::TYPE_LOOP) {
                visitLoop(stmt->loop);
            } else {
                visitVariable(stmt->variable);
            }
            currentTempVariableAllocator()->release(tempIndex);
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