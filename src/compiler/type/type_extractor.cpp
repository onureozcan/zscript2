#include <compiler/type_meta.h>
#include <compiler/op.h>
#include <common/logger.h>

#include <vector>

using namespace std;

namespace zero {

    class TypeMetadataExtractor::Impl {
    public:
        TypeMetadataRepository *typeMetadataRepository;

        void extractAndRegister(FunctionAstNode *function) {
            visitFunction(function);
        }

    private:
        struct LocalPropertyPointer {
            int depth;
            TypeInfo::PropertyDescriptor *descriptor;
        };

        Logger log = Logger("type_extractor");

        TypeInfo *currentContext = nullptr;
        BaseAstNode *currentAstNode = nullptr;
        vector<TypeInfo *> contextStack;

        void errorExit(string error) {
            log.error(error.c_str());
            exit(1);
        }

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        void addContext(FunctionAstNode *function) {
            auto newContext = new TypeInfo("function_context@" +
                                           function->fileName + "(" + to_string(function->line) + "&" +
                                           to_string(function->pos) + ")");
            function->typeName = newContext->name;
            typeMetadataRepository->registerType(newContext);
            if (currentContext != nullptr)
                newContext->addProperty("$parent", currentContext);
            contextStack.push_back(newContext);
            currentContext = contextStack[contextStack.size() - 1];
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

        void visitVariable(VariableAstNode *variable) {
            currentAstNode = variable;
            if (variable->initialValue == nullptr) {
                currentContext->addProperty(variable->identifier, &TypeInfo::ANY);
            } else {
                visitExpression(variable->initialValue);
                currentContext->addProperty(variable->identifier, typeOrError(variable->initialValue->typeName));
            }
        }

        LocalPropertyPointer findPropertyInContextChainOrError(string name) {
            int depth = 0;
            TypeInfo *current = currentContext;
            while (current != nullptr && depth < contextStack.size()) {
                auto descriptor = current->getProperty(name);
                if (descriptor != nullptr) {
                    return {depth, descriptor};
                }
                current = contextStack[contextStack.size() - depth - 1];
                depth++;
            }
            errorExit("cannot find variable " + name + currentNodeInfoStr());
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
                string returnTypeStr = Operator::getReturnType(op, {binary->left->typeName, binary->right->typeName});
                binary->typeName = returnTypeStr;
            } catch (runtime_error e) {
                errorExit(e.what() + currentNodeInfoStr());
            }
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
            binary->typeName = propertyDescriptor->name;
            binary->memoryIndex = propertyDescriptor->index;
            binary->isLvalue = right->isLvalue;
        }

        void visitExpression(ExpressionAstNode *expression) {
            switch (expression->expressionType) {
                case ExpressionAstNode::TYPE_ATOMIC : {
                    visitAtom((AtomicExpressionAstNode *) expression);
                    break;
                }
                case ExpressionAstNode::TYPE_BINARY: {
                    visitBinary((BinaryExpressionAstNode *) expression);
                }
            }
        }

        void visitFunction(FunctionAstNode *function) {
            currentAstNode = function;
            addContext(function);
            auto statements = function->program->statements;
            for (auto &stmt: *statements) {
                if (stmt->type == StatementAstNode::TYPE_EXPRESSION) {
                    visitExpression(stmt->expression);
                } else {
                    visitVariable(stmt->variable);
                }
            }
        }
    };

    void TypeMetadataExtractor::extractAndRegister(FunctionAstNode *function) {
        this->impl->extractAndRegister(function);
    }

    TypeMetadataExtractor::TypeMetadataExtractor(TypeMetadataRepository *repository) {
        this->impl = new Impl();
        this->impl->typeMetadataRepository = repository;
    }

}