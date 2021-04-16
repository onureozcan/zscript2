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

        TypeInfo *getOrRegisterFunctionType(vector<string> argTypes, string returnType, int isNative = 0) {
            string argsStr;
            for (const auto &argType: argTypes) {
                argsStr += argType + ",";
            }
            auto typeName = string(isNative ? "native" : "fun") + "<" + argsStr + returnType + ">";
            TypeInfo *type = typeMetadataRepository->findTypeByName(typeName);
            if (type == nullptr) {
                type = new TypeInfo(typeName, 1, isNative);
                for (auto &argType:argTypes) {
                    type->addParameter(typeOrError(argType));
                }
                type->addParameter(typeOrError(returnType));
                typeMetadataRepository->registerType(type);
            }
            return type;
        }

        TypeInfo *typeOrError(string name) {
            auto typeInfo = typeMetadataRepository->findTypeByName(name);
            if (typeInfo == nullptr) {
                errorExit("unknown type `" + name + "` " + currentNodeInfoStr());

            }
            return typeInfo;
        }

        Operator *opOrError(string name, int operandCount) {
            auto op = Operator::getBy(name, operandCount);
            if (op != nullptr) {
                return op;
            }
            errorExit("there is no such operator named `" + name + "` and takes " + to_string(operandCount) +
                      " operand(s) " + currentNodeInfoStr());
        }

        LocalPropertyPointer findPropertyInContextChainOrError(string name) {
            int depth = 0;
            TypeInfo *current = currentContext();
            while (true) {
                auto descriptor = current->getProperty(name);
                if (descriptor != nullptr) {
                    return {depth, descriptor};
                }
                depth++;
                if (depth <= contextStack.size())
                    current = contextStack[contextStack.size() - depth];
                else break;
            }
            errorExit("cannot find variable " + name + currentNodeInfoStr());
        }

        TypeInfo::PropertyDescriptor *findPropertyInObjectOrError(ExpressionAstNode *left, string name) {
            auto typeOfLeft = typeOrError(left->typeName);
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
            auto expectedType = typeOrError(variable->typeName);
            auto selectedType = expectedType;
            if (variable->initialValue == nullptr) {
                variable->memoryIndex = parentContext->addProperty(variable->identifier, expectedType);
                log.debug("variable `%s`:`%s` added to context %s", variable->identifier.c_str(),
                          expectedType->name.c_str(), parentContext->name.c_str());
            } else {
                visitExpression(variable->initialValue);
                auto initializedType = typeOrError(variable->initialValue->typeName);
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
                variable->typeName = selectedType->name;
            }
            log.debug("\n\tvar `%s`:`%s` added to context %s", variable->identifier.c_str(),
                      selectedType->name.c_str(), parentContext->name.c_str());
        }

        void visitAtom(AtomicExpressionAstNode *atomic) {
            currentAstNode = atomic;
            switch (atomic->atomicType) {
                case AtomicExpressionAstNode::TYPE_DECIMAL: {
                    atomic->typeName = TypeInfo::DECIMAL.name;
                    break;
                }
                case AtomicExpressionAstNode::TYPE_INT: {
                    atomic->typeName = TypeInfo::INT.name;
                    break;
                }
                case AtomicExpressionAstNode::TYPE_STRING: {
                    atomic->typeName = TypeInfo::STRING.name;
                    break;
                }
                case AtomicExpressionAstNode::TYPE_FUNCTION: {
                    visitFunction((FunctionAstNode *) atomic);
                    break;
                }
                case AtomicExpressionAstNode::TYPE_IDENTIFIER: {
                    LocalPropertyPointer depthTypeInfo = findPropertyInContextChainOrError(atomic->data);
                    atomic->typeName = depthTypeInfo.descriptor->typeInfo->name;
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
                string returnTypeStr = Operator::getReturnType(op, binary->left->typeName, binary->right->typeName);
                binary->typeName = returnTypeStr;
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
            binary->right->typeName = propertyDescriptor->name;
            binary->right->memoryIndex = propertyDescriptor->index;
            binary->typeName = propertyDescriptor->name;
            binary->isLvalue = right->isLvalue;
        }

        void visitUnary(PrefixExpressionAstNode *unary) {
            currentAstNode = unary;
            visitExpression(unary->right);
            auto op = opOrError(unary->opName, 1);
            try {
                string returnTypeStr = Operator::getReturnType(op, unary->right->typeName);
                unary->typeName = returnTypeStr;
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
            auto calleeType = typeOrError(call->left->typeName);
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
                auto givenType = typeOrError(call->params->at(i)->typeName);
                if (!expectedType->isAssignableFrom(givenType)) {
                    errorExit("cannot pass type `" + givenType->name + "` as parameter to arg of type `" +
                              expectedType->name + "` " + currentNodeInfoStr());
                }
            }

            // the last parameter is the return type
            auto returnType = expectedParameterTypes.at(expectedParameterTypes.size() - 1);
            call->typeName = returnType->name;
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
                    visitUnary((PrefixExpressionAstNode *) expression);
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
            // native print function
            currentContext()->addProperty("print",
                                          getOrRegisterFunctionType({TypeInfo::STRING.name}, TypeInfo::T_VOID.name, 1));
            // native to string function
            TypeInfo::INT.addProperty("toString", getOrRegisterFunctionType({}, TypeInfo::STRING.name, 1));
            TypeInfo::DECIMAL.addProperty("toString", getOrRegisterFunctionType({}, TypeInfo::STRING.name, 1));

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

            auto returnTypeName = TypeInfo::T_VOID.name;
            if (stmt->expression != nullptr) {
                visitExpression(stmt->expression);
                returnTypeName = stmt->expression->typeName;
            }

            auto functionType = typeOrError(currentFunction->typeName);
            auto expectedReturnType = functionType->getParameters().back();

            if (!expectedReturnType->isAssignableFrom(typeOrError(returnTypeName))) {
                errorExit("cannot return `" + returnTypeName + "` from a function that returns `" +
                          expectedReturnType->name + "` " + currentNodeInfoStr());
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

        void visitFunction(FunctionAstNode *function) {
            functionsStack.push_back(function);
            currentAstNode = function;
            addContext(function->program);

            // register arguments to current context
            for (const auto &piece : *function->arguments) {
                currentContext()->addProperty(piece.first, typeOrError(piece.second));
                log.debug("\n\targument `%s`:`%s` added to context %s", piece.first.c_str(),
                          piece.second.c_str(), currentContext()->name.c_str());
            }

            // create and register function type
            vector<string> argTypes;
            for (const auto &piece : *function->arguments) {
                argTypes.push_back(piece.second);
            }

            TypeInfo *functionType = getOrRegisterFunctionType(argTypes, function->returnTypeName);
            function->typeName = functionType->name;

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