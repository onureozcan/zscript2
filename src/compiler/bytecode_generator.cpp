#include <common/program.h>
#include <compiler/compiler.h>
#include <common/logger.h>

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

        unsigned int size() {
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

        unsigned int releaseTemp(unsigned int variableIndex) {
            tempVariableOccupancyMap[variableIndex] = 0;
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
        BaseAstNode *currentAstNode = nullptr;

        Program *rootProgram = nullptr;
        vector<Program *> subroutinePrograms; // list of subroutines created so far

        vector<Program *> programsStack; // to keep track of current program
        vector<FunctionAstNode *> functionAstStack; // to keep track of current function ast
        vector<TempVariableAllocator> tempVariableAllocatorStack;

        Program *currentProgram() {
            if (programsStack.empty()) return nullptr;
            return programsStack.back();
        }

        FunctionAstNode *currentFunctionAst() {
            if (functionAstStack.empty()) return nullptr;
            return functionAstStack.back();
        }

        TempVariableAllocator currentTempVariableAllocator() {
            return tempVariableAllocatorStack.back();
        }

        void errorExit(string error) {
            log.error(error.c_str());
            exit(1);
        }

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        TypeInfo *type(string name) {
            return typeMetadataRepository->findTypeByName(name);
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
            return sub;
        }

        unsigned int visitFunction(FunctionAstNode *function,
                                   unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            // -- entry
            currentAstNode = function;

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
            tempVariableAllocatorStack.emplace_back(contextObjectType);

            // --- function body

            for (auto &stmt:*function->program->statements) {
                visitStatement(stmt);
            }

            // ----- exit

            auto tempVariableAllocator = currentTempVariableAllocator();

            unsigned int functionContextObjectSize =
                    contextObjectType->getPropertyCount() + tempVariableAllocator.size();

            currentProgram()->addInstructionAt(
                    (new Instruction())->withOpCode(
                                    currentFunctionAst()->isLeafFunction ? FN_ENTER_STACK : FN_ENTER_HEAP)
                            ->withOp1(functionContextObjectSize)
                            ->withComment("allocate call frame that is " + to_string(functionContextObjectSize) +
                                          " values big"),
                    *programEntryLabel);

            tempVariableAllocatorStack.pop_back();
            programsStack.pop_back();
            functionAstStack.pop_back();

            return preferredIndex;
        }

        unsigned int visitAtom(AtomicExpressionAstNode *atomic,
                               unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            currentAstNode = atomic;
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    currentProgram()->addInstruction(
                            (new Instruction())->withOpCode(MOV)
                                    ->withOpType(DECIMAL)
                                    ->withOp1((float) atof(atomic->data.c_str()))
                                    ->withDestination(preferredIndex)
                                    ->withComment("load int into index " + to_string(preferredIndex) +
                                                  " in the current frame")
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
                                                  " in the current frame")
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
                                                  " in the current frame")
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

        unsigned int visitExpression(ExpressionAstNode *expression,
                                     unsigned int preferredIndex = 0 /* 0 is null value, means no specific destination request*/
        ) {
            switch (expression->expressionType) {
                case ExpressionAstNode::TYPE_ATOMIC : {
                    return visitAtom((AtomicExpressionAstNode *) expression, preferredIndex);
                }
                case ExpressionAstNode::TYPE_BINARY: {
                    //return visitBinary((BinaryExpressionAstNode *) expression);
                    break;
                }
                case ExpressionAstNode::TYPE_UNARY: {
                    //return visitUnary((PrefixExpressionAstNode *) expression);
                    break;
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
            if (stmt->expression != nullptr) {
                valueIndex = visitExpression(stmt->expression);
            }
            currentProgram()->addInstruction(
                    (new Instruction())->withOpCode(RET)
                            ->withDestination(valueIndex)
                            ->withComment("return value at " + to_string(valueIndex))
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