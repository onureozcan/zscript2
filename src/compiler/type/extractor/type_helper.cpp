#include <compiler/type_meta.h>

namespace zero {

    TypeInfo *TypeHelper::getParametricType(const string &name) {
        int depth = 0;
        TypeInfo *current;
        while (true) {
            if (depth < contextChain->size())
                current = contextChain->at(depth);
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

    TypeInfo *TypeHelper::typeOrError(const string &name, map<string, TypeInfo *> *typeArguments) {
        auto typeInfo = typeMetadataRepository->findTypeByName(name);
        if (typeInfo == nullptr) {
            if (typeArguments != nullptr && typeArguments->find(name) != typeArguments->end()) {
                typeInfo = typeArguments->find(name)->second;
                return typeInfo;
            }
            typeInfo = getParametricType(name);
            if (typeInfo != nullptr) {
                return typeInfo;
            }
            throw TypeExtractionException("`" + name + "` does not name a type or type parameter ");
        }
        return typeInfo;
    }

    TypeInfo *TypeHelper::typeOrError(TypeDescriptorAstNode *typeAst,
                                      map<string, TypeInfo *> *typeArguments) {
        // 1 - check that such a type exists considering number of parameters
        auto name = typeAst->name;
        auto paramCount = typeAst->parameters.size();
        if (name == "fun" || name == "native") {
            // function is a special case. it can one to have infinite number of parameters and is always exists
            return getFunctionTypeFromTypeAst(typeAst, typeArguments);
        }
        auto foundType = typeOrError(name, typeArguments);
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
                    throw TypeExtractionException(
                            "type bounds check failed for parameterized type ` " + typeAst->toString() + "`: `" +
                            typeBoundary->name +
                            "` is not assignable from `" +
                            paramAsType->name + "` ");
                }
            }
            clone->addTypeArgument(typeBoundaryIdent, paramAsType);
        }
        return clone;
    }

    static TypeDescriptorAstNode *getTypeAstFromFunctionProperties(
            const vector<TypeDescriptorAstNode *> &argTypes,
            TypeDescriptorAstNode *returnType,
            int isNative) {
        auto typeAst = new TypeDescriptorAstNode();
        typeAst->name = isNative ? "native" : "fun";
        for (auto argType : argTypes) {
            typeAst->parameters.push_back(argType);
        }
        typeAst->parameters.push_back(returnType);
        return typeAst;
    }

    TypeInfo *TypeHelper::getFunctionTypeFromTypeAst(TypeDescriptorAstNode *typeAst,
                                                     map<string, TypeInfo *> *typeArguments) {
        auto functionType = new TypeInfo("fun", true, false);
        auto paramCount = typeAst->parameters.size();
        if (paramCount == 0) {
            throw TypeExtractionException("return type expected for function type");
        }
        for (int i = 0; i < paramCount; i++) {
            auto paramAsAst = typeAst->parameters.at(i);
            auto paramAsType = typeOrError(paramAsAst, typeArguments);
            functionType->addFunctionArgument(paramAsType);
        }
        return functionType;
    }

    TypeInfo *TypeHelper::getFunctionTypeFromFunctionSignature(
            const vector<TypeDescriptorAstNode *> &argTypes,
            TypeDescriptorAstNode *returnType,
            map<string, TypeInfo *> *signatureTypeArguments,
            int isNative) {

        auto typeAst = getTypeAstFromFunctionProperties(argTypes, returnType, isNative);
        auto type = getFunctionTypeFromTypeAst(typeAst, signatureTypeArguments);
        type->isNative = isNative;
        return type;
    }

    TypeInfo *TypeHelper::getFunctionTypeFromFunctionAst(FunctionAstNode *function) {
        // create type arguments
        map<string, TypeInfo *> typeArguments;
        for (auto &piece: function->typeArguments) {
            auto name = piece.first;
            auto typeAst = piece.second;
            auto typeBoundary = typeOrError(typeAst);
            auto typeArgument = new TypeInfo(name, typeBoundary->isCallable, typeBoundary->isNative, true);
            typeArgument->typeBoundary = typeBoundary;
            typeArguments.insert({name, typeArgument});
        }

        vector<TypeDescriptorAstNode *> argTypes;
        for (const auto &piece : *function->arguments) {
            argTypes.push_back(piece.second);
        }

        TypeInfo *functionType = getFunctionTypeFromFunctionSignature(argTypes,
                                                                      function->returnType,
                                                                      &typeArguments);
        for (const auto &typeArg: typeArguments) {
            functionType->addTypeArgument(typeArg.first, typeArg.second);
        }
        return functionType;
    }

    void TypeHelper::addTypeArgumentToCurrentContext(const vector<pair<string, TypeDescriptorAstNode *>> &typeAstMap) {
        TypeInfo *context = contextChain->current();
        for (auto &piece: typeAstMap) {
            auto name = piece.first;
            auto typeAst = piece.second;
            auto typeBoundary = typeOrError(typeAst);
            auto typeArgument = new TypeInfo(name, typeBoundary->isCallable, typeBoundary->isNative, true);
            typeArgument->typeBoundary = typeBoundary;
            context->addTypeArgument(name, typeArgument);
        }
    }
}