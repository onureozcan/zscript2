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

        void errorExit(string error) {
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

        TypeInfo *typeOrError(string name, int paramCount = 0) {
            auto typeInfo = typeMetadataRepository->findTypeByName(name, paramCount);
            if (typeInfo == nullptr) {
                errorExit("unknown type `" + name + "` " + currentNodeInfoStr());
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
                functionType->addParameter(paramAsType);
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
            auto foundType = typeOrError(name, paramCount);
            if (paramCount == 0) return foundType;

            // if it contains type parameters, create a copy with parameters checked and bound
            auto clone = new TypeInfo(foundType->name, foundType->isCallable, foundType->isNative);
            clone->clonePropertiesFrom(foundType);

            // 2- check that every type parameters in the ast exists
            for (int i = 0; i < paramCount; i++) {
                auto paramAsAst = typeAst->parameters.at(i);
                auto paramAsType = typeOrError(paramAsAst);
                auto typeBoundry = foundType->getParameters().at(i);
                if (typeBoundry->name != paramAsType->name) {
                    if (!typeBoundry->isAssignableFrom(paramAsType)) {
                        errorExit("type bounds check failed for parameterized type ` " + typeAst->toString() + "`: `" +
                                  typeBoundry->name +
                                  "` is not assignable from `" +
                                  paramAsType->name + "` " +
                                  currentNodeInfoStr());
                    }
                }
                clone->addParameter(paramAsType);
            }

            return clone;
        }

        Operator *opOrError(string name, int operandCount) {
            auto op = Operator::getBy(name, operandCount);
            if (op != nullptr) {
                return op;
            }
            errorExit("there is no such operator named `" + name + "` and takes " + to_string(operandCount) +
                      " operand(s) " + currentNodeInfoStr());
        }


        TypeDescriptorAstNode *typeAstFromTypeInfo(TypeInfo *typeInfo) {
            auto typeAst = new TypeDescriptorAstNode();
            typeAst->name = typeInfo->isNative ? "native" : typeInfo->name;
            auto parameters = typeInfo->getParameters();
            for (auto &parameter : parameters) {
                typeAst->parameters.push_back(typeAstFromTypeInfo(parameter));
            }
            return typeAst;
        }

        LocalPropertyPointer findPropertyInContextChainOrError(string name) {
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

        TypeInfo::PropertyDescriptor *findPropertyInObjectOrError(ExpressionAstNode *left, string name) {
            auto typeOfLeft = typeOrError(left->typeDescriptorAstNode);
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
                auto initializedType = typeOrError(variable->initialValue->typeDescriptorAstNode);
                if (variable->hasExplicitTypeInfo) {
                    if (expectedType->isAssignableFrom(initializedType)) {
                        selectedType = expectedType;
                    } else {
                        errorExit("cannot assign `" + initializedType->name + "` to `" + expectedType->name + "`" +
                                  currentNodeInfoStr());
                    }
                } else {
                    selectedType = initializedType;
                    variable->typeDescriptorAstNode = variable->initialValue->typeDescriptorAstNode;
                }
                variable->memoryIndex = parentContext->addProperty(variable->identifier, selectedType);
            }
            log.debug("\n\tvar `%s`:`%s` added to context %s", variable->identifier.c_str(),
                      selectedType->name.c_str(), parentContext->name.c_str());
        }

        /**
         * This converts immediate values into known properties in the current context
         * so that MOV_{{IMMEDIATE_TYPE}} instructions can be avoided
         * @param atomic
         * @param type
         */
        void convertImmediateToLocalVariable(AtomicExpressionAstNode *atomic, TypeDescriptorAstNode *typeAst) {
            auto propertyName = atomic->data;
            unsigned int memoryIndex;
            auto type = typeOrError(typeAst);
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
            atomic->typeDescriptorAstNode = typeAst;
            atomic->atomicType = AtomicExpressionAstNode::TYPE_IDENTIFIER;
            atomic->data = localProperty->name;
        }

        void visitAtom(AtomicExpressionAstNode *atomic) {
            currentAstNode = atomic;
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    convertImmediateToLocalVariable(atomic, TypeDescriptorAstNode::from(TypeInfo::DECIMAL.name));
                    break;
                }
                case AtomicExpressionAstNode::TYPE_INT: {
                    convertImmediateToLocalVariable(atomic, TypeDescriptorAstNode::from(TypeInfo::INT.name));
                    break;
                }
                case AtomicExpressionAstNode::TYPE_BOOLEAN: {
                    convertImmediateToLocalVariable(atomic, TypeDescriptorAstNode::from(TypeInfo::BOOLEAN.name));
                    break;
                }
                case AtomicExpressionAstNode::TYPE_STRING: {
                    atomic->typeDescriptorAstNode = TypeDescriptorAstNode::from(TypeInfo::STRING.name);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_FUNCTION: {
                    visitFunction((FunctionAstNode *) atomic);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_IDENTIFIER: {
                    LocalPropertyPointer depthTypeInfo = findPropertyInContextChainOrError(atomic->data);
                    atomic->typeDescriptorAstNode = typeAstFromTypeInfo(depthTypeInfo.descriptor->typeInfo);
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
            Operator *op = opOrError(binary->opName, 2);
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
                auto type1 = typeOrError(binary->left->typeDescriptorAstNode);
                auto type2 = typeOrError(binary->right->typeDescriptorAstNode);
                string returnTypeStr = Operator::getReturnType(op, type1->name, type2->name);
                binary->typeDescriptorAstNode = TypeDescriptorAstNode::from(returnTypeStr);
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
            auto accessResultTypeAst = typeAstFromTypeInfo(propertyDescriptor->typeInfo);
            binary->right->typeDescriptorAstNode = accessResultTypeAst;
            binary->right->memoryIndex = propertyDescriptor->index;
            binary->typeDescriptorAstNode = accessResultTypeAst;
            binary->isLvalue = right->isLvalue;
        }

        void visitPrefix(PrefixExpressionAstNode *prefix) {
            currentAstNode = prefix;
            visitExpression(prefix->right);
            auto op = opOrError(prefix->opName, 1);
            try {
                auto type = typeOrError(prefix->right->typeDescriptorAstNode);
                string returnTypeStr = Operator::getReturnType(op, type->name);
                prefix->typeDescriptorAstNode = TypeDescriptorAstNode::from(returnTypeStr);
            } catch (runtime_error e) {
                errorExit(e.what() + currentNodeInfoStr());
            }
        }

        void visitFunctionCall(FunctionCallExpressionAstNode *call) {
            visitExpression(call->left);
            for (auto parameter:*call->params) {
                visitExpression(parameter);
            }
            currentAstNode = call;

            // check if it is callable
            auto calleeType = typeOrError(call->left->typeDescriptorAstNode);
            if (!calleeType->isCallable) {
                errorExit("type `" + calleeType->name + "` is not callable " + currentNodeInfoStr());
            }
            auto expectedParameterTypes = calleeType->getParameters();
            auto expectedParameterCount = expectedParameterTypes.size() - 1;
            if (expectedParameterCount != call->params->size()) {
                errorExit("expected " + to_string(expectedParameterCount) + " args, given " +
                          to_string(call->params->size()) + "" + currentNodeInfoStr());
            }
            for (unsigned int i = 0; i < call->params->size(); i++) {
                auto expectedType = expectedParameterTypes[i];
                auto givenType = typeOrError(call->params->at(i)->typeDescriptorAstNode);
                if (!expectedType->isAssignableFrom(givenType)) {
                    errorExit("cannot pass type `" + givenType->name + "` as parameter to arg of type `" +
                              expectedType->name + "` " + currentNodeInfoStr());
                }
            }

            // the last parameter is the return type
            auto returnType = expectedParameterTypes.at(expectedParameterTypes.size() - 1);
            call->typeDescriptorAstNode = typeAstFromTypeInfo(returnType);
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
                returnType = typeOrError(stmt->expression->typeDescriptorAstNode);
            }

            auto functionType = typeOrError(currentFunction->typeDescriptorAstNode);
            auto expectedReturnType = functionType->getParameters().back();

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
                if (loop->loopConditionExpression->typeDescriptorAstNode->name != TypeInfo::BOOLEAN.name) {
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
            function->typeDescriptorAstNode = typeAstFromTypeInfo(functionType);

            // visit children
            auto statements = function->program->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }

            contextStack.pop_back();
            functionsStack.pop_back();
        }
    };

    void TypeMetadataExtractor::extractAndRegister(ProgramAstNode *function) {
        this->impl->extractAndRegister(function);
    }

    TypeMetadataExtractor::TypeMetadataExtractor(TypeMetadataRepository *repository) {
        this->impl = new Impl();
        this->impl->typeMetadataRepository = repository;
    }
}