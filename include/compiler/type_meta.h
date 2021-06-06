#pragma once

#include <string>

#include <compiler/ast.h>
#include <compiler/type.h>

using namespace std;

namespace zero {
    class TypeInfoRepository {
        class Impl;

    public:

        static TypeInfoRepository *getInstance();

        void registerType(TypeInfo *type);

        TypeInfo *findTypeByName(const string &name);

    private:
        TypeInfoRepository();

        static TypeInfoRepository *instance;

        Impl *impl;
    };

    class TypeExtractionException : public std::runtime_error {
    public:
        explicit TypeExtractionException(const string &arg) : runtime_error(arg) {}
    };

    class TypeInfoExtractor {
        class Impl;

    public:
        void extractAndRegister(ProgramAstNode *program);

        explicit TypeInfoExtractor();

    private:
        Impl *impl;
    };

    class ContextChain {
    private:
        vector<TypeInfo *> contextStack;
        TypeInfoRepository *typeInfoRepository = TypeInfoRepository::getInstance();

    public:
        TypeInfo *current();

        void push(ProgramAstNode *ast, string name = "");

        void pop();

        int size();

        TypeInfo *at(int depth);
    };

    class TypeHelper {
    private:
        TypeInfoRepository *typeInfoRepository = TypeInfoRepository::getInstance();
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

        TypeInfo *getOverloadToCall(ExpressionAstNode *callee, vector<TypeInfo *> *typeParameters,
                                    vector<TypeInfo *> *functionParameters);

        TypeInfo *getClassType(ClassDeclarationAstNode *pNode);

        TypeInfo *
        getClassLevelFunctionTypeFromFunctionAst(FunctionAstNode *function, map<string, TypeInfo *> classTypeArguments);
    };
}