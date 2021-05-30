#pragma once

#include <string>

#include <compiler/ast.h>
#include <compiler/type.h>

using namespace std;

namespace zero {
    class TypeMetadataRepository {
        class Impl;

    public:

        static TypeMetadataRepository *getInstance();

        void registerType(TypeInfo *type);

        TypeInfo *findTypeByName(const string &name);

    private:
        TypeMetadataRepository();

        static TypeMetadataRepository *instance;

        Impl *impl;
    };

    class TypeExtractionException : public std::runtime_error {
    public:
        explicit TypeExtractionException(const string &arg) : runtime_error(arg) {}
    };

    class TypeMetadataExtractor {
        class Impl;

    public:
        void extractAndRegister(ProgramAstNode *program);

        explicit TypeMetadataExtractor();

    private:
        Impl *impl;
    };

    class ContextChain {
    private:
        vector<TypeInfo *> contextStack;
        TypeMetadataRepository *typeMetadataRepository = TypeMetadataRepository::getInstance();

    public:
        TypeInfo *current();

        void push(ProgramAstNode *ast);

        void pop();

        int size();

        TypeInfo *at(int depth);
    };

    class TypeHelper {
    private:
        TypeMetadataRepository *typeMetadataRepository = TypeMetadataRepository::getInstance();
        ContextChain *contextChain;

    public:
        explicit TypeHelper(ContextChain *contextChain) {
            this->contextChain = contextChain;
        }

        void addTypeArgumentToCurrentContext(const vector<pair<string, TypeDescriptorAstNode *>> &typeAstMap);

        TypeInfo *getParametricType(const string &name);

        TypeInfo *typeOrError(const string &name, map<string, TypeInfo *> *typeArguments = nullptr);

        TypeInfo *typeOrError(TypeDescriptorAstNode *typeAst, map<string, TypeInfo *> *typeArguments = nullptr);

        TypeInfo *
        getFunctionTypeFromTypeAst(TypeDescriptorAstNode *typeAst, map<string, TypeInfo *> *typeArguments = nullptr);

        TypeInfo *getFunctionTypeFromFunctionSignature(
                const vector<TypeDescriptorAstNode *> &argTypes,
                TypeDescriptorAstNode *returnType,
                map<string, TypeInfo *> *typeArguments = nullptr,
                int isNative = false);

        TypeInfo *getFunctionTypeFromFunctionAst(FunctionAstNode *function);
    };
}