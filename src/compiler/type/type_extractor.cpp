#include <compiler/type_meta.h>

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

        TypeInfo *currentContext = nullptr;
        BaseAstNode *currentAstNode = nullptr;
        vector<TypeInfo *> contextStack;

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        void addContext(FunctionAstNode *function) {
            auto newContext = new TypeInfo(
                    function->fileName + "@" + to_string(function->line) + "&" + to_string(function->pos));
            function->typeName = newContext->name;
            if (currentContext != nullptr)
                newContext->addProperty("$parent", currentContext);
            contextStack.push_back(newContext);
            currentContext = contextStack[contextStack.size() - 1];
        }

        TypeInfo *typeOrError(string identifier) {
            auto typeInfo = typeMetadataRepository->findTypeByName(identifier);
            if (typeInfo == nullptr) {
                throw runtime_error("unknown type `" + identifier + "` " + currentNodeInfoStr());
            }
            return typeInfo;
        }

        void visitVariable(VariableAstNode *variable) {
            currentAstNode = variable;
            if (variable->initialValue != nullptr) {
                currentContext->addProperty(variable->identifier, &TypeInfo::ANY);
            } else {
                visitExpression(variable->initialValue);
                currentContext->addProperty(variable->identifier, typeOrError(variable->initialValue->typeName));
            }
        }

        LocalPropertyPointer findPropertyInContextChainOrError(string name) {
            int depth = 0;
            TypeInfo *current = currentContext;
            while (current != nullptr) {
                auto descriptor = current->getProperty(name);
                if (descriptor != nullptr) {
                    return {depth, descriptor};
                }
                current = contextStack[contextStack.size() - depth - 1];
            }
            throw runtime_error("cannot find variable " + name + currentNodeInfoStr());
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

        void visitBinary(BinaryExpressionAstNode *binary) {
            currentAstNode = binary;

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