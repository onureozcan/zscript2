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
        TypeInfoRepository *typeInfoRepository = nullptr;

        Program *generate(ProgramAstNode *programAstNode) {
            return doGenerateCode(programAstNode);
        }

    private:

        typedef struct {
            string *loopBodyLabel;
            string *loopEndLabel;
            string *loopConditionLabel;
            string *loopIterationLabel;
        } LoopLabelInfoStruct;

        Logger log = Logger("bytecode_generator");

        Program *rootProgram = nullptr;
        vector<Program *> subroutinePrograms; // list of subroutines created so far

        vector<Program *> programsStack; // to keep track of current program
        vector<FunctionAstNode *> functionAstStack; // to keep track of current function ast

        vector<LoopLabelInfoStruct> loopsStack; // this is useful to generate break and continue codes

        map<string, TempVariableAllocator *> tempVariableAllocatorMap;

        void errorExit(const string &error) {
            log.error(error.c_str());
            exit(1);
        }

        LoopLabelInfoStruct *currentLoopLabelSet() {
            if (loopsStack.empty()) return nullptr;
            return &loopsStack.back();
        };

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

        TypeInfo *type(const string &name) const {
            return typeInfoRepository->findTypeByName(name);
        }

        static Operator *getOp(string name, int operandCount) {
            return Operator::getBy(std::move(name), operandCount);
        }

        Program *doGenerateCode(ProgramAstNode *programAstNode) {
            auto globalFnc = new FunctionAstNode();
            globalFnc->program = programAstNode;
            globalFnc->arguments = new vector<pair<string, TypeDescriptorAstNode *>>();
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

        void generateMovImmediate(const string &immediateData, const string &typeName, unsigned int preferredIndex) {
            if (typeName == TypeInfo::INT.name) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_INT)
                                ->withOp1((unsigned) (stoi(immediateData)))
                                ->withDestination(preferredIndex)
                                ->withComment("load int into index " + to_string(preferredIndex) +
                                              " in the current frame - " + immediateData)
                );
            } else if (typeName == TypeInfo::BOOLEAN.name) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_BOOLEAN)
                                ->withOp1((unsigned) (immediateData == "true" ? 1 : 0))
                                ->withDestination(preferredIndex)
                                ->withComment("load boolean into index " + to_string(preferredIndex) +
                                              " in the current frame - " + immediateData)
                );
            } else if (typeName == TypeInfo::DECIMAL.name) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(MOV_DECIMAL)
                                ->withOp1((float) atof(immediateData.c_str()))
                                ->withDestination(preferredIndex)
                                ->withComment("load decimal into index " + to_string(preferredIndex) +
                                              " in the current frame - " + immediateData)
                );
            }
        }

        /**
         * Immediate values require MOV_INT like instructions everytime they are being used.
         * To reduce the overhead, all the immediates are manually extracted a variable into the current context
         * this function moves them all at once at the beginning of the function
         * @param typeInfo
         */
        void generateImmediates(TypeInfo *typeInfo) {
            for (const auto &immediatePropertyInfo: typeInfo->getImmediateProperties()) {
                auto immediateName = immediatePropertyInfo.first;
                auto immediateData = immediatePropertyInfo.second;
                auto immediatePropertyDescriptor = typeInfo->getProperty(immediateName);
                auto immediateType = immediatePropertyDescriptor->firstOverload().type;
                auto preferredIndex = immediatePropertyDescriptor->firstOverload().index;

                generateMovImmediate(immediateData, immediateType->name, preferredIndex);
            }
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
            generateImmediates(contextObjectType);

            // --- function body
            for (int i = 0; i < function->arguments->size(); i++) {
                auto argPair = function->arguments->at(i);
                auto argIndex = contextObjectType->getProperty(argPair.first)->firstOverload().index;
                currentProgram()->addInstruction(
                        (new Instruction())
                                ->withOpCode(ARG_READ)
                                ->withOp1((unsigned int) (function->arguments->size() - i - 1))
                                ->withDestination(argIndex)
                                ->withComment(
                                        "getting argument at index " + to_string(i) + " into " + to_string(argIndex))
                );
            }

            visitProgram(function->program);

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
                case AtomicExpressionAstNode::TYPE_DECIMAL:
                case AtomicExpressionAstNode::TYPE_INT:
                case AtomicExpressionAstNode::TYPE_BOOLEAN: {
                    generateMovImmediate(atomic->data, atomic->resolvedType->name, preferredIndex);
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
            auto functionType = functionCall->left->resolvedType;
            auto opCode = functionType->isNative ? CALL_NATIVE : CALL;

            unsigned int functionIndex = visitExpression(functionCall->left, preferredIndex);
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(opCode)
                            ->withOp1(functionIndex)
                            ->withOp2(functionCall->params->size())
                            ->withDestination(preferredIndex)
                            ->withComment("calling functionCall at index " + to_string(functionIndex))
            );

            return preferredIndex;
        }

        unsigned int visitAssignment(BinaryExpressionAstNode *binary, unsigned int preferredIndex) {

            if (binary->left->expressionType == ExpressionAstNode::TYPE_ATOMIC) {
                // assign without DOT operation
                unsigned int memoryDepth = binary->left->memoryDepth;
                unsigned int memoryIndex = binary->left->memoryIndex;

                unsigned int valueIndex = visitExpression(binary->right, memoryIndex);

                if (memoryDepth == 0) {
                    // set in current context
                    if (valueIndex != memoryIndex) {
                        currentProgram()->addInstruction(
                                (new Instruction())->withOpCode(MOV)
                                        ->withOp1(valueIndex)
                                        ->withDestination(memoryIndex)
                                        ->withComment("mov value at index " + to_string(valueIndex) + " into index " +
                                                      to_string(memoryIndex) + " in the current frame")
                        );
                    }
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

            return preferredIndex;
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
                                ->withComment("short circuit or v1 branch, dest is " + to_string(preferredIndex))
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
                                ->withOp1(actualValueIndex2)
                                ->withDestination(preferredIndex)
                                ->withComment("short circuit or v2 branch, dest is " + to_string(preferredIndex))
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
                return visitAssignment(binary, preferredIndex);
            } else if (op == &Operator::AND) {
                return visitAnd(binary, preferredIndex);
            } else if (op == &Operator::OR) {
                return visitOr(binary, preferredIndex);
            } else {
                unsigned int tempValueIndex1 = currentTempVariableAllocator()->alloc();
                unsigned int tempValueIndex2 = currentTempVariableAllocator()->alloc();
                unsigned int decimalTempIndex = currentTempVariableAllocator()->alloc();

                unsigned int actualValueIndex1 = visitExpression(binary->left, tempValueIndex1);
                unsigned int actualValueIndex2 = visitExpression(binary->right, tempValueIndex2);

                unsigned short opCode = 0;
                auto typeOfBinary = binary->resolvedType;
                auto isDecimalOp = binary->left->resolvedType->name == TypeInfo::DECIMAL.name ||
                                   binary->right->resolvedType->name == TypeInfo::DECIMAL.name;

                if (isDecimalOp) {
                    // auto casting
                    if (binary->left->resolvedType->name == TypeInfo::INT.name) {
                        currentProgram()->addInstruction(
                                (new Instruction())->withOpCode(CAST_DECIMAL)
                                        ->withOp1(actualValueIndex1)
                                        ->withDestination(decimalTempIndex)
                                        ->withComment("auto cast from int to decimal")
                        );
                        if (actualValueIndex1 != tempValueIndex1) {
                            currentTempVariableAllocator()->release(tempValueIndex1);
                        }
                        actualValueIndex1 = decimalTempIndex;
                    }
                    if (binary->right->resolvedType->name == TypeInfo::INT.name) {
                        currentProgram()->addInstruction(
                                (new Instruction())->withOpCode(CAST_DECIMAL)
                                        ->withOp1(actualValueIndex2)
                                        ->withDestination(decimalTempIndex)
                                        ->withComment("auto cast from int to decimal")
                        );
                        if (actualValueIndex1 != tempValueIndex1) {
                            currentTempVariableAllocator()->release(tempValueIndex2);
                        }
                        actualValueIndex2 = decimalTempIndex;
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

                if (actualValueIndex1 != tempValueIndex1) {
                    currentTempVariableAllocator()->release(tempValueIndex1);
                }
                if (actualValueIndex2 != tempValueIndex2) {
                    currentTempVariableAllocator()->release(tempValueIndex2);
                }
                currentTempVariableAllocator()->release(decimalTempIndex);
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
                if (prefix->resolvedType->name == TypeInfo::DECIMAL.name) {
                    opCode = Opcode::NEG_DECIMAL;
                } else {
                    opCode = Opcode::NEG_INT;
                }
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(opCode)
                                ->withOp1(actualValueIndex)
                                ->withDestination(preferredIndex)
                                ->withComment(
                                        "took " + op->name + " of value at index " + to_string(actualValueIndex) +
                                        "and put it into " + to_string(preferredIndex) + " in the current frame")
                );
            } else if (op == &Operator::NOT) {
                // TODO
            }
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

        void cast(unsigned int valueIndex, unsigned int destinationIndex, TypeInfo *t1, TypeInfo *t2) {
            if (t1->name != t2->name) {
                if (t1 == &TypeInfo::INT && t2 == &TypeInfo::DECIMAL) {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(CAST_DECIMAL)
                                    ->withOp1(valueIndex)
                                    ->withDestination(destinationIndex)
                                    ->withComment("auto cast fromm int to float")
                    );
                } else {
                    errorExit("cannot cast from " + t1->name + " to " + t2->name);
                }
            }
        }

        void visitVariable(VariableAstNode *variable) {
            auto destinationIndex = variable->memoryIndex;
            auto expectedType = variable->resolvedType;

            if (variable->initialValue != nullptr) {
                unsigned int requestedValueIndex = destinationIndex;
                unsigned int actualValueIndex = visitExpression(variable->initialValue, requestedValueIndex);

                auto actualType = variable->initialValue->resolvedType;
                cast(actualValueIndex, destinationIndex, actualType, expectedType);

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
                // NULL initializer
                auto typeObj = variable->resolvedType;
                auto opCode = typeObj == &TypeInfo::DECIMAL ? MOV_DECIMAL : MOV_INT;
                auto initialValue = typeObj == &TypeInfo::DECIMAL ? 0.0f : 0;
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(opCode)
                                ->withOp1(initialValue)
                                ->withDestination(destinationIndex)
                                ->withComment("mov NULL value into index " +
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

            visitProgram(ifStatementAstNode->program);

            if (ifStatementAstNode->elseProgram != nullptr) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(JMP)
                                ->withDestination(ifEndLabel)
                                ->withComment("if goto end")
                );
            }
            currentProgram()->addLabel(ifFalseLabel);
            if (ifStatementAstNode->elseProgram != nullptr) {
                visitProgram(ifStatementAstNode->elseProgram);
            }
            currentProgram()->addLabel(ifEndLabel);
        }

        void visitLoop(LoopAstNode *loop) {
            auto loopBodyLabel = new string("__loop_body__" + to_string(loop->line) + "_" + to_string(loop->pos));
            auto loopEndLabel = new string("__loop_end__" + to_string(loop->line) + "_" + to_string(loop->pos));
            auto loopConditionLabel = new string(
                    "__loop_condition__" + to_string(loop->line) + "_" + to_string(loop->pos));
            auto loopIterationLabel = new string(
                    "__loop_iteration__" + to_string(loop->line) + "_" + to_string(loop->pos));

            loopsStack.push_back({
                                         loopBodyLabel, loopEndLabel, loopConditionLabel, loopIterationLabel
                                 });

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
            visitProgram(loop->program);

            currentProgram()->addLabel(loopIterationLabel);
            if (loop->loopIterationExpression != nullptr) {
                actualLoopIterationResultIndex = visitExpression(loop->loopIterationExpression,
                                                                 loopIterationResultTempIndex);
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

            loopsStack.pop_back();

            currentTempVariableAllocator()->release(loopConditionTempIndex);
            currentTempVariableAllocator()->release(loopIterationResultTempIndex);
        }

        void visitBreak(StatementAstNode *statementAstNode) {
            LoopLabelInfoStruct *currentLoopLabels = currentLoopLabelSet();
            if (currentLoopLabels != nullptr) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(JMP)
                                ->withDestination(currentLoopLabels->loopEndLabel)
                );
            } else {
                errorExit("not inside a label " + statementAstNode->fileName + " at line " +
                          to_string(statementAstNode->line));
            }
        }

        void visitContinue(StatementAstNode *statementAstNode) {
            LoopLabelInfoStruct *currentLoopLabels = currentLoopLabelSet();
            if (currentLoopLabels != nullptr) {
                currentProgram()->addInstruction(
                        (new Instruction())->withOpCode(JMP)
                                ->withDestination(currentLoopLabels->loopIterationLabel)
                );
            } else {
                errorExit("not inside a label " + statementAstNode->fileName + " at line " +
                          to_string(statementAstNode->line));
            }
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
            } else if (stmt->type == StatementAstNode::TYPE_BREAK) {
                visitBreak(stmt);
            } else if (stmt->type == StatementAstNode::TYPE_CONTINUE) {
                visitContinue(stmt);
            } else if (stmt->type == StatementAstNode::TYPE_VARIABLE_DECLARATION) {
                visitVariable(stmt->variable);
            }
            currentTempVariableAllocator()->release(tempIndex);
        }

        void visitNamedFunctions(ProgramAstNode *program) {
            auto statements = program->statements;
            for (auto stmt: statements) {
                if (stmt->type == StatementAstNode::TYPE_NAMED_FUNCTION) {
                    visitFunction(stmt->namedFunction, stmt->namedFunction->memoryIndex);
                }
            }
        }

        void visitProgram(ProgramAstNode *program) {
            visitNamedFunctions(program);
            auto statements = program->statements;
            for (auto stmt: statements) {
                visitStatement(stmt);
            }
        }
    };

    Program *ByteCodeGenerator::generate(ProgramAstNode *programAstNode) {
        return impl->generate(programAstNode);
    }

    ByteCodeGenerator::ByteCodeGenerator() {
        this->impl = new ByteCodeGenerator::Impl();
        this->impl->typeInfoRepository = TypeInfoRepository::getInstance();
    }
}