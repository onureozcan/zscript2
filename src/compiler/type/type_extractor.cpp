#include <compiler/type_meta.h>
#include <compiler/op.h>
#include <common/logger.h>

#include <vector>

using namespace std;

namespace zero {

    class TypeMetadataExtractor::Impl {
    public:
        TypeMetadataRepository *typeMetadataRepository;

        void extractAndRegister(ProgramAstNode *program) {
            visitGlobal(program);
        }

    private:
        struct LocalPropertyPointer {
            int depth;
            TypeInfo::PropertyDescriptor *descriptor;
        };

        Logger log = Logger("type_extractor");

        BaseAstNode *currentAstNode = nullptr;
        vector<TypeInfo *> contextStack;
        vector<FunctionAstNode *> functionsStack;

        void errorExit(const string &error) {
            log.error(error.c_str());
            exit(1);
        }

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        TypeInfo *currentContext() {
            if (contextStack.empty()) return nullptr;
            return contextStack.back();
        }

        void addTypeArguments(const vector<pair<string, TypeDescriptorAstNode *>> &typeAstMap) {
            TypeInfo *context = currentContext();
            for (auto &piece: typeAstMap) {
                auto name = piece.first;
                auto typeAst = piece.second;
                auto typeBoundary = typeOrError(typeAst);
                auto typeArgument = new TypeInfo(name, typeBoundary->isCallable, typeBoundary->isNative, true);
                typeArgument->typeBoundary = typeBoundary;
                context->addTypeArgument(name, typeArgument);
            }
        }

        TypeInfo *getParametricType(const string &name) {
            int depth = 0;
            TypeInfo *current;
            while (true) {
                if (depth < contextStack.size())
                    current = contextStack[contextStack.size() - depth - 1];
                else break;
                auto typeArguments = current->getTypeArguments();
                for (const auto &argument: typeArguments) {
                    if (argument.first == name) {
                        return argument.second;
                    }
                }
                depth++;
            }
            return nullptr;
        }

        TypeInfo *getFunctionType(const vector<TypeDescriptorAstNode *> &argTypes,
                                  TypeDescriptorAstNode *returnType,
                                  int isNative = false) {
            auto typeAst = TypeDescriptorAstNode();
            typeAst.name = isNative ? "native" : "fun";
            for (auto argType : argTypes) {
                typeAst.parameters.push_back(argType);
            }
            typeAst.parameters.push_back(returnType);
            auto type = constructFunctionTypeFromAst(&typeAst);
            type->isNative = isNative;
            return type;
        }

        TypeInfo *typeOrError(const string &name) {
            auto typeInfo = typeMetadataRepository->findTypeByName(name);
            if (typeInfo == nullptr) {
                typeInfo = getParametricType(name);
                if (typeInfo != nullptr) {
                    return typeInfo;
                }
                errorExit("`" + name + "` does not name a type or type parameter " + currentNodeInfoStr());
            }
            return typeInfo;
        }

        TypeInfo *constructFunctionTypeFromAst(TypeDescriptorAstNode *typeAst) {
            auto functionType = new TypeInfo("fun", true, false);
            auto paramCount = typeAst->parameters.size();
            if (paramCount == 0) {
                errorExit("return type expected for function type " + currentNodeInfoStr());
            }
            for (int i = 0; i < paramCount; i++) {
                auto paramAsAst = typeAst->parameters.at(i);
                auto paramAsType = typeOrError(paramAsAst);
                functionType->addFunctionArgument(paramAsType);
            }
            return functionType;
        }

        TypeInfo *typeOrError(TypeDescriptorAstNode *typeAst) {
            // 1 - check that such a type exists considering number of parameters
            auto name = typeAst->name;
            auto paramCount = typeAst->parameters.size();
            if (name == "fun" || name == "native") {
                // function is a special case. it can have infinite number of parameters and is always exists
                return constructFunctionTypeFromAst(typeAst);
            }
            auto foundType = typeOrError(name);
            if (paramCount == 0) return foundType;

            // if it contains type parameters, create a copy with parameters checked and bound
            auto clone = new TypeInfo(foundType->name, foundType->isCallable, foundType->isNative);
            clone->clonePropertiesFrom(foundType);

            // 2- check that every type parameters in the ast exists
            for (int i = 0; i < paramCount; i++) {
                auto paramAsAst = typeAst->parameters.at(i);
                auto paramAsType = typeOrError(paramAsAst);
                auto typeBoundaryPair = foundType->getTypeArguments().at(i);
                auto typeBoundary = typeBoundaryPair.second;
                auto typeBoundaryIdent = typeBoundaryPair.first;
                if (typeBoundary->name != paramAsType->name) {
                    if (!typeBoundary->isAssignableFrom(paramAsType)) {
                        errorExit("type bounds check failed for parameterized type ` " + typeAst->toString() + "`: `" +
                                  typeBoundary->name +
                                  "` is not assignable from `" +
                                  paramAsType->name + "` " +
                                  currentNodeInfoStr());
                    }
                }
                clone->addTypeArgument(typeBoundaryIdent, paramAsType);
            }

            return clone;
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
                if (depth < contextStack.size())
                    current = contextStack[contextStack.size() - depth - 1];
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
            auto parentContext = currentContext();
            auto expectedType = typeOrError(variable->typeDescriptorAstNode);
            auto selectedType = expectedType;
            if (variable->initialValue == nullptr) {
                variable->memoryIndex = parentContext->addProperty(variable->identifier, expectedType);
                log.debug("variable `%s`:`%s` added to context %s", variable->identifier.c_str(),
                          expectedType->name.c_str(), parentContext->name.c_str());
            } else {
                visitExpression(variable->initialValue);
                auto initializedType = variable->initialValue->resolvedType;
                if (variable->hasExplicitTypeInfo) {
                    if (expectedType->isAssignableFrom(initializedType)) {
                        selectedType = expectedType;
                    } else {
                        errorExit("cannot assign `" + initializedType->name + "` to `" + expectedType->name + "`" +
                                  currentNodeInfoStr());
                    }
                } else {
                    selectedType = initializedType;
                }
                variable->memoryIndex = parentContext->addProperty(variable->identifier, selectedType);
            }
            variable->resolvedType = selectedType;
            log.debug("\n\tvar `%s`:`%s` added to context %s", variable->identifier.c_str(),
                      selectedType->name.c_str(), parentContext->name.c_str());
        }

        /**
         * This converts immediate values into known properties in the current context
         * so that MOV_{{IMMEDIATE_TYPE}} instructions can be avoided
         * @param atomic
         * @param type
         */
        void convertImmediateToLocalVariable(AtomicExpressionAstNode *atomic, TypeInfo *type) {
            auto propertyName = atomic->data;
            unsigned int memoryIndex;
            auto localProperty = currentContext()->getImmediate(propertyName, type);
            if (localProperty == nullptr) {
                memoryIndex = currentContext()->addImmediate(propertyName, type);
                log.debug("converted immediate value %s into property, stored in index %d", atomic->data.c_str(),
                          memoryIndex);
                localProperty = currentContext()->getImmediate(propertyName, type);
            } else {
                memoryIndex = localProperty->index;
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
                    atomic->resolvedType = depthTypeInfo.descriptor->typeInfo;
                    atomic->memoryDepth = depthTypeInfo.depth;
                    atomic->memoryIndex = depthTypeInfo.descriptor->index;
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
            binary->right->resolvedType = propertyDescriptor->typeInfo;
            binary->right->memoryIndex = propertyDescriptor->index;
            binary->resolvedType = propertyDescriptor->typeInfo;
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
                auto paramAsType = typeOrError(paramAsAst);
                call->resolvedTypeParams.push_back(paramAsType);
            }

            visitExpression(call->left);
            for (auto parameter:*call->params) {
                visitExpression(parameter);
            }
            currentAstNode = call;

            // check if it is callable
            auto calleeType = call->left->resolvedType;
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


        void addContext(ProgramAstNode *ast) {
            auto newContext = new TypeInfo("FunContext@" +
                                           ast->fileName + "(" + to_string(ast->line) + "&" +
                                           to_string(ast->pos) + ")", 0);

            typeMetadataRepository->registerType(newContext);
            ast->contextObjectTypeName = newContext->name;
            if (currentContext() != nullptr)
                newContext->addProperty("$parent", currentContext());
            contextStack.push_back(newContext);
        }

        void visitGlobal(ProgramAstNode *program) {
            currentAstNode = program;
            addContext(program);

            currentContext()->addProperty("$parent", &TypeInfo::ANY);

            // native print function
            currentContext()->addProperty("print", getFunctionType({TypeDescriptorAstNode::from(TypeInfo::ANY.name)},
                                                                   TypeDescriptorAstNode::from(TypeInfo::T_VOID.name),
                                                                   true));

            auto statements = program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }
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

            auto statements = loop->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
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
            }
        }

        void visitIfStatement(IfStatementAstNode *ifStatementAstNode) {
            visitExpression(ifStatementAstNode->expression);
            auto statements = ifStatementAstNode->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }
            if (ifStatementAstNode->elseProgram != nullptr) {
                statements = ifStatementAstNode->elseProgram->statements;
                for (auto stmt: *statements) {
                    visitStatement(stmt);
                }
            }
        }

        void visitFunction(FunctionAstNode *function) {
            functionsStack.push_back(function);
            currentAstNode = function;
            addContext(function->program);
            addTypeArguments(function->typeArguments);
            // register arguments to current context
            for (const auto &piece : *function->arguments) {
                currentContext()->addProperty(piece.first, typeOrError(piece.second));
                log.debug("\n\targument `%s`:`%s` added to context %s", piece.first.c_str(),
                          piece.second->toString().c_str(), currentContext()->name.c_str());
            }

            // create and register function type
            vector<TypeDescriptorAstNode *> argTypes;
            for (const auto &piece : *function->arguments) {
                argTypes.push_back(piece.second);
            }

            TypeInfo *functionType = getFunctionType(argTypes, function->returnType);
            for (const auto &typeArg: currentContext()->getTypeArguments()) {
                functionType->addTypeArgument(typeArg.first, typeArg.second);
            }
            function->resolvedType = functionType;

            // visit children
            auto statements = function->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }

            //typeParametersStack.pop_back();
            contextStack.pop_back();
            functionsStack.pop_back();
        }
    };

    void TypeMetadataExtractor::extractAndRegister(ProgramAstNode *function) {
        this->impl->extractAndRegister(function);
    }

    TypeMetadataExtractor::TypeMetadataExtractor() {
        this->impl = new Impl();
        this->impl->typeMetadataRepository = TypeMetadataRepository::getInstance();
    }
}