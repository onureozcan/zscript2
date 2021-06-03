#include <compiler/type_meta.h>
#include <compiler/op.h>

#include <vector>

using namespace std;

namespace zero {

    class TypeInfoExtractor::Impl {

    private:
        struct LocalPropertyPointer {
            int depth;
            TypeInfo::PropertyDescriptor *descriptor;
        };

        Logger log = Logger("type_extractor");

        ContextChain contextChain;
        TypeHelper typeHelper = TypeHelper(&contextChain);

        BaseAstNode *currentAstNode = nullptr;
        vector<FunctionAstNode *> functionsStack;

        void errorExit(const string &error) {
            log.error(error.c_str());
            exit(1);
        }

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        Operator *opOrError(const string &name, int operandCount) {
            auto op = Operator::getBy(name, operandCount);
            if (op != nullptr) {
                return op;
            }
            errorExit("there is no such operator named `" + name + "` and takes " + to_string(operandCount) +
                      " operand(s) " + currentNodeInfoStr());
            return nullptr;
        }

        LocalPropertyPointer findPropertyInContextChainOrError(const string &name) {
            int depth = 0;
            TypeInfo *current;
            while (true) {
                if (depth < contextChain.size())
                    current = contextChain.at(depth);
                else break;

                auto descriptor = current->getProperty(name);
                if (descriptor != nullptr) {
                    return {depth, descriptor};
                }
                depth++;
            }
            errorExit("cannot find variable " + name + currentNodeInfoStr());
            return {0, nullptr};
        }

        TypeInfo::PropertyDescriptor *findPropertyInObjectOrError(ExpressionAstNode *left, const string &name) {
            auto typeOfLeft = left->resolvedType;
            auto typeInfo = typeOfLeft->getProperty(name);
            if (typeInfo == nullptr) {
                errorExit("object type `" + typeOfLeft->name + "` does not have property named `" + name + "`" +
                          currentNodeInfoStr());
            }
            return typeInfo;
        }

        void visitVariable(VariableAstNode *variable) {
            currentAstNode = variable;
            auto parentContext = contextChain.current();
            auto expectedType = typeHelper.typeOrError(variable->typeDescriptorAstNode);
            auto selectedType = expectedType;
            if (variable->initialValue == nullptr) {
                variable->memoryIndex = parentContext->addProperty(variable->identifier, expectedType);
            } else {
                visitExpression(variable->initialValue);
                auto initializedType = variable->initialValue->resolvedType;
                if (variable->hasExplicitTypeInfo) {
                    if (expectedType->isAssignableFrom(initializedType)) {
                        selectedType = expectedType;
                    } else {
                        errorExit("cannot assign `" + initializedType->toString() + "` to `" + expectedType->toString() + "`" +
                                  currentNodeInfoStr());
                    }
                } else {
                    selectedType = initializedType;
                }
                variable->memoryIndex = parentContext->addProperty(variable->identifier, selectedType);
            }
            variable->resolvedType = selectedType;
        }

        /**
         * This converts immediate values into known properties in the current context
         * so that MOV_{{IMMEDIATE_TYPE}} instructions can be avoided
         * @param atomic
         * @param type
         */
        void convertImmediateToLocalVariable(AtomicExpressionAstNode *atomic, TypeInfo *type) {
            auto currentContext = contextChain.current();
            auto propertyName = atomic->data;
            unsigned int memoryIndex;
            auto localProperty = currentContext->getImmediate(propertyName, type);
            if (localProperty == nullptr) {
                memoryIndex = currentContext->addImmediate(propertyName, type);
                log.debug("converted immediate value %s into property, stored in index %d", atomic->data.c_str(),
                          memoryIndex);
                localProperty = currentContext->getImmediate(propertyName, type);
            } else {
                memoryIndex = localProperty->firstOverload().index;
                log.debug("found immediate value %s at index %d, using it instead", atomic->data.c_str(), memoryIndex);
            }
            atomic->memoryIndex = memoryIndex;
            atomic->memoryDepth = 0;
            atomic->resolvedType = type;
            atomic->atomicType = AtomicExpressionAstNode::TYPE_IDENTIFIER;
            atomic->data = localProperty->name;
        }

        void visitAtom(AtomicExpressionAstNode *atomic) {
            currentAstNode = atomic;
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    convertImmediateToLocalVariable(atomic, &TypeInfo::DECIMAL);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_INT: {
                    convertImmediateToLocalVariable(atomic, &TypeInfo::INT);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_BOOLEAN: {
                    convertImmediateToLocalVariable(atomic, &TypeInfo::BOOLEAN);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_STRING: {
                    atomic->resolvedType = &TypeInfo::STRING;
                    break;
                }
                case AtomicExpressionAstNode::TYPE_FUNCTION: {
                    visitFunction((FunctionAstNode *) atomic);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_IDENTIFIER: {
                    LocalPropertyPointer depthTypeInfo = findPropertyInContextChainOrError(atomic->data);
                    atomic->resolvedType = depthTypeInfo.descriptor->firstOverload().type;
                    atomic->memoryDepth = depthTypeInfo.depth;
                    atomic->memoryIndex = depthTypeInfo.descriptor->firstOverload().index;
                    atomic->propertyInfo = depthTypeInfo.descriptor;
                    break;
                }
            }
        }

        void visitAssign(BinaryExpressionAstNode *binary) {
            visitExpression(binary->left);
            visitExpression(binary->right);
            if (!binary->left->isLvalue) {
                errorExit("lvalue expected for assignment." + currentNodeInfoStr());
            }
            if (binary->left->isOverloaded() || binary->right->isOverloaded()) {
                errorExit("overloaded types are not explicitly assignable." + currentNodeInfoStr());
            }
            if (!binary->left->resolvedType->isAssignableFrom(binary->right->resolvedType)) {
                errorExit("cannot assign from type "
                          + binary->left->resolvedType->toString()
                          + " to " + binary->right->resolvedType->toString()
                          + currentNodeInfoStr());
            }
        }

        void visitBinary(BinaryExpressionAstNode *binary) {
            currentAstNode = binary;
            auto op = opOrError(binary->opName, 2);
            if (op == &Operator::DOT) {
                visitDot(binary);
                return;
            } else if (op == &Operator::ASSIGN) {
                visitAssign(binary);
            } else {
                visitExpression(binary->left);
                visitExpression(binary->right);
            }
            try {
                TypeInfo *type1 = binary->left->resolvedType;
                TypeInfo *type2 = binary->right->resolvedType;
                TypeInfo *returnType = Operator::getReturnType(op, type1, type2);
                binary->resolvedType = returnType;
            } catch (runtime_error e) {
                errorExit(e.what() + currentNodeInfoStr());
            }
        }

        void visitDot(BinaryExpressionAstNode *binary) {
            visitExpression(binary->left);
            if (binary->right->expressionType != ExpressionAstNode::TYPE_ATOMIC
                && ((AtomicExpressionAstNode *) binary->right)->atomicType !=
                   AtomicExpressionAstNode::TYPE_IDENTIFIER) {
                errorExit("right operand of the dot operator must be an identifier." + currentNodeInfoStr());
            }
            auto *right = (AtomicExpressionAstNode *) binary->right;
            auto propertyNameToSearch = right->data;
            auto propertyDescriptor = findPropertyInObjectOrError(binary->left, propertyNameToSearch);
            binary->right->resolvedType = propertyDescriptor->firstOverload().type;
            binary->right->memoryIndex = propertyDescriptor->firstOverload().index;
            binary->right->propertyInfo = propertyDescriptor;
            binary->resolvedType = binary->right->resolvedType;
            binary->propertyInfo = propertyDescriptor;
            binary->isLvalue = right->isLvalue;
        }

        void visitPrefix(PrefixExpressionAstNode *prefix) {
            currentAstNode = prefix;
            visitExpression(prefix->right);
            auto op = opOrError(prefix->opName, 1);
            try {
                auto type = prefix->right->resolvedType;
                TypeInfo *returnType = Operator::getReturnType(op, type);
                prefix->resolvedType = returnType;
            } catch (runtime_error e) {
                errorExit(e.what() + currentNodeInfoStr());
            }
        }

        void visitFunctionCall(FunctionCallExpressionAstNode *call) {

            // this holds the parametric type name - passed type map (to discover actual return type)
            map<string, TypeInfo *> passedTypesMap;

            // resolve type parameters
            for (const auto &paramAsAst: call->typeParams) {
                auto paramAsType = typeHelper.typeOrError(paramAsAst);
                call->resolvedTypeParams.push_back(paramAsType);
            }

            vector<TypeInfo *> functionParameterTypes;
            visitExpression(call->left);
            for (auto parameter:*call->params) {
                visitExpression(parameter);
                functionParameterTypes.push_back(parameter->resolvedType);
            }
            currentAstNode = call;

            // check if it is callable
            TypeInfo *calleeType;
            try {
                calleeType = typeHelper.getOverloadToCall(
                        call->left, &call->resolvedTypeParams, &functionParameterTypes
                );
                call->preferredCalleeOverload = calleeType;
            } catch (TypeExtractionException &ex) {
                errorExit(ex.what() + currentNodeInfoStr());
            }

            if (!calleeType->isCallable) {
                errorExit("type `" + calleeType->name + "` is not callable " + currentNodeInfoStr());
            }

            // check type parameters
            auto expectedTypeParameterTypes = calleeType->getTypeArguments();
            auto expectedTypeParameterCount = expectedTypeParameterTypes.size();
            auto explicitlyGivenParameterCount = call->resolvedTypeParams.size();

            if (explicitlyGivenParameterCount != 0) {
                // parameter types are explicitly passed. counts must match!
                if (explicitlyGivenParameterCount != expectedTypeParameterCount) {
                    errorExit("expected " + to_string(expectedTypeParameterCount) + " parameter types. passed "
                              + to_string(explicitlyGivenParameterCount) + " " + currentNodeInfoStr());
                }
            }

            for (unsigned int i = 0; i < explicitlyGivenParameterCount; i++) {
                auto expectedTypeParam = expectedTypeParameterTypes.at(i);
                auto givenTypeParam = call->resolvedTypeParams.at(i);
                if (expectedTypeParam.second->typeBoundary->isAssignableFrom(givenTypeParam)) {
                    passedTypesMap[expectedTypeParam.first] = givenTypeParam;
                } else {
                    errorExit("type argument `" + expectedTypeParam.first
                              + "` is not assignable from " + givenTypeParam->toString() + " " + currentNodeInfoStr());
                }
            }

            // check function parameters
            auto expectedFunctionParameterTypes = calleeType->getFunctionArguments();
            auto expectedParameterCount = expectedFunctionParameterTypes.size() - 1;
            if (expectedParameterCount != call->params->size()) {
                errorExit("expected " + to_string(expectedParameterCount) + " args, given " +
                          to_string(call->params->size()) + "" + currentNodeInfoStr());
            }
            for (unsigned int i = 0; i < call->params->size(); i++) {
                auto expectedType = expectedFunctionParameterTypes[i];
                auto givenType = call->params->at(i)->resolvedType;

                if (expectedType->isTypeArgument) {
                    // if a parameterized type is passed, it can be inferred automatically
                    // of course, if it is not passed explicitly
                    TypeInfo *passedTypeParam;
                    if (passedTypesMap.find(expectedType->name) != passedTypesMap.end()) {
                        passedTypeParam = passedTypesMap[expectedType->name];
                    } else {
                        passedTypeParam = givenType;
                    }
                    if (expectedType->typeBoundary->isAssignableFrom(passedTypeParam)) {
                        passedTypesMap[expectedType->name] = passedTypeParam;
                        expectedType = expectedType->typeBoundary;
                    } else {
                        errorExit("type argument `" + expectedType->name
                                  + "` is not assignable from " + passedTypeParam->toString() + " " +
                                  currentNodeInfoStr());
                    }
                }

                if (!expectedType->isAssignableFrom(givenType)) {
                    errorExit("cannot pass type `" + givenType->name + "` as parameter to arg of type `" +
                              expectedType->name + "` " + currentNodeInfoStr());
                }
            }
            // make sure that all the type parameters are linked
            if (passedTypesMap.size() != expectedTypeParameterCount) {
                errorExit("expected " + to_string(expectedTypeParameterCount) + " parameter types. passed "
                          + to_string(passedTypesMap.size()) + " " + currentNodeInfoStr());
            }

            // the last parameter is the return type
            auto returnType = expectedFunctionParameterTypes.at(expectedFunctionParameterTypes.size() - 1);

            // resolve type parameters
            returnType = returnType->resolveGenericType(&passedTypesMap);

            call->resolvedType = returnType;
        }

        void visitExpression(ExpressionAstNode *expression) {
            switch (expression->expressionType) {
                case ExpressionAstNode::TYPE_ATOMIC : {
                    visitAtom((AtomicExpressionAstNode *) expression);
                    break;
                }
                case ExpressionAstNode::TYPE_BINARY: {
                    visitBinary((BinaryExpressionAstNode *) expression);
                    break;
                }
                case ExpressionAstNode::TYPE_UNARY: {
                    visitPrefix((PrefixExpressionAstNode *) expression);
                    break;
                }
                case ExpressionAstNode::TYPE_FUNCTION_CALL: {
                    visitFunctionCall((FunctionCallExpressionAstNode *) expression);
                    break;
                }
            }
        }

        void visitGlobal(ProgramAstNode *program) {
            currentAstNode = program;
            contextChain.push(program);

            auto currentContext = contextChain.current();

            currentContext->addProperty("$parent", &TypeInfo::ANY);

            // native print function
            currentContext->addProperty("print", typeHelper.getFunctionTypeFromFunctionSignature(
                    {TypeDescriptorAstNode::from(TypeInfo::ANY.name)},
                    TypeDescriptorAstNode::from(TypeInfo::T_VOID.name),
                    nullptr, true));

            visitProgram(program);
        }

        void visitReturn(StatementAstNode *stmt) {
            currentAstNode = stmt;
            if (functionsStack.empty()) {
                errorExit("return outside of function " + currentNodeInfoStr());
            }

            auto currentFunction = functionsStack.at(functionsStack.size() - 1);

            auto returnType = &TypeInfo::T_VOID;
            if (stmt->expression != nullptr) {
                visitExpression(stmt->expression);
                returnType = stmt->expression->resolvedType;
            }

            auto functionType = currentFunction->resolvedType;
            auto expectedReturnType = functionType->getFunctionArguments().back();

            if (!expectedReturnType->isAssignableFrom(returnType)) {
                errorExit("cannot return `" + returnType->name + "` from a function that returns `" +
                          expectedReturnType->name + "` " + currentNodeInfoStr());
            }
        }

        void visitLoop(LoopAstNode *loop) {
            if (loop->loopVariable != nullptr) {
                visitVariable(loop->loopVariable);
            }
            if (loop->loopConditionExpression != nullptr) {
                visitExpression(loop->loopConditionExpression);
                if (loop->loopConditionExpression->resolvedType->name != TypeInfo::BOOLEAN.name) {
                    errorExit("a boolean expression as a loop condition was expected" + currentNodeInfoStr());
                }
            }
            if (loop->loopIterationExpression != nullptr) {
                visitExpression(loop->loopIterationExpression);
            }

            visitProgram(loop->program);
        }

        void visitNamedFunctions(ProgramAstNode *program) {
            for (auto &statement: program->statements) {
                if (statement->type == StatementAstNode::TYPE_NAMED_FUNCTION) {
                    currentAstNode = statement;
                    auto function = statement->namedFunction;
                    auto functionType = typeHelper.getFunctionTypeFromFunctionAst(function);
                    function->resolvedType = functionType;
                    try {
                        function->memoryIndex = contextChain.current()->addProperty(function->name, functionType, true);
                    } catch (runtime_error &err) {
                        errorExit(err.what() + currentNodeInfoStr());
                    }
                }
            }
        }

        void visitStatement(StatementAstNode *stmt) {
            if (stmt->type == StatementAstNode::TYPE_EXPRESSION) {
                visitExpression(stmt->expression);
            } else if (stmt->type == StatementAstNode::TYPE_RETURN) {
                visitReturn(stmt);
            } else if (stmt->type == StatementAstNode::TYPE_IF) {
                visitIfStatement(stmt->ifStatement);
            } else if (stmt->type == StatementAstNode::TYPE_LOOP) {
                visitLoop(stmt->loop);
            } else if (stmt->type == StatementAstNode::TYPE_VARIABLE_DECLARATION) {
                visitVariable(stmt->variable);
            } else if (stmt->type == StatementAstNode::TYPE_NAMED_FUNCTION) {
                visitFunction(stmt->namedFunction);
            }
        }

        void visitProgram(ProgramAstNode *program) {
            visitNamedFunctions(program);
            for (auto stmt: program->statements) {
                visitStatement(stmt);
            }
        }

        void visitIfStatement(IfStatementAstNode *ifStatementAstNode) {
            visitExpression(ifStatementAstNode->expression);
            visitProgram(ifStatementAstNode->program);

            if (ifStatementAstNode->elseProgram != nullptr) {
                visitProgram(ifStatementAstNode->elseProgram);
            }
        }

        void visitFunction(FunctionAstNode *function) {

            if (function->resolvedType == nullptr) {
                function->resolvedType = typeHelper.getFunctionTypeFromFunctionAst(function);
            }
            functionsStack.push_back(function);
            currentAstNode = function;

            contextChain.push(function->program);

            typeHelper.addTypeArgumentToCurrentContext(function->typeArguments);

            // add arguments to current context as properties
            for (const auto &piece : *function->arguments) {
                contextChain.current()->addProperty(piece.first, typeHelper.typeOrError(piece.second));
            }

            // visit children
            visitProgram(function->program);

            contextChain.pop();
            functionsStack.pop_back();
        }

    public:
        void extractAndRegister(ProgramAstNode *program) {
            visitGlobal(program);
        }
    };

    void TypeInfoExtractor::extractAndRegister(ProgramAstNode *function) {
        this->impl->extractAndRegister(function);
    }

    TypeInfoExtractor::TypeInfoExtractor() {
        this->impl = new Impl();
    }
}