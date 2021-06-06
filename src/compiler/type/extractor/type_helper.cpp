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
        auto typeInfo = typeInfoRepository->findTypeByName(name);
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

        auto requiredArgsCount = foundType->getTypeArguments().size();
        if (requiredArgsCount != paramCount) {
            throw TypeExtractionException("type " + foundType->name + " requires "
                                          + to_string(requiredArgsCount) + " type args. given " +
                                          to_string(paramCount));
        }

        // 2- check that every type parameters in the ast exists
        for (int i = 0; i < paramCount; i++) {
            auto paramAsAst = typeAst->parameters.at(i);
            auto paramAsType = typeOrError(paramAsAst, typeArguments);
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
        for (const auto &piece : function->arguments) {
            argTypes.push_back(piece.second);
        }

        TypeInfo *functionType = getFunctionTypeFromFunctionSignature(argTypes,
                                                                      function->returnType,
                                                                      &typeArguments);
        for (auto &piece: function->typeArguments) {
            auto name = piece.first;
            auto typeArg = typeArguments[name];
            functionType->addTypeArgument(name, typeArg);
        }
        return functionType;
    }

    TypeInfo *TypeHelper::getClassLevelFunctionTypeFromFunctionAst(
            FunctionAstNode *function, map<string,
            TypeInfo *> classTypeArguments) {

        vector<TypeDescriptorAstNode *> argTypes;
        for (const auto &piece : function->arguments) {
            argTypes.push_back(piece.second);
        }

        TypeInfo *functionType = getFunctionTypeFromFunctionSignature(argTypes,
                                                                      function->returnType,
                                                                      &classTypeArguments);
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

    TypeInfo *TypeHelper::getOverloadToCall(
            ExpressionAstNode *callee,
            vector<TypeInfo *> *typeParameters,
            vector<TypeInfo *> *functionParameters) {

        // not overloaded
        if (callee->propertyInfo == nullptr) {
            return callee->resolvedType;
        }

        auto overloads = callee->propertyInfo->allOverloads();
        // not overloaded
        if (overloads.size() == 1) {
            return overloads.front().type;
        }

        auto explicitlyPassedTypeParameterCount = typeParameters->size();

        // filter by type parameters
        auto filteredByExplicitTypeParameters = vector<TypeInfo *>();
        if (explicitlyPassedTypeParameterCount != 0) {
            for (auto overload: overloads) {
                auto calleeType = overload.type;
                auto expectedTypeParameterCount = calleeType->getTypeArguments().size();
                if (expectedTypeParameterCount == explicitlyPassedTypeParameterCount) {
                    //check type parameters
                    auto expectedTypeParameters = calleeType->getTypeArguments();
                    auto filtered = false;
                    for (unsigned int i = 0; i < explicitlyPassedTypeParameterCount; i++) {
                        auto expectedTypeParam = expectedTypeParameters.at(i);
                        auto givenTypeParam = typeParameters->at(i);
                        if (!expectedTypeParam.second->typeBoundary->isAssignableFrom(givenTypeParam)) {
                            filtered = true;
                            break;
                        }
                    }
                    if (!filtered)
                        filteredByExplicitTypeParameters.push_back(calleeType);
                }
            }
        } else {
            for (auto overload: overloads) {
                filteredByExplicitTypeParameters.push_back(overload.type);
            }
        }

        // filter by function parameters
        auto filteredByFunctionParameters = vector<TypeInfo *>();
        for (auto calleeType: filteredByExplicitTypeParameters) {
            auto expectedFunctionParameterTypes = calleeType->getFunctionArguments();
            auto expectedParameterCount = expectedFunctionParameterTypes.size() - 1;
            if (expectedParameterCount != functionParameters->size()) {
                continue;
            }
            auto filtered = false;
            for (unsigned int i = 0; i < functionParameters->size(); i++) {
                auto expectedType = expectedFunctionParameterTypes[i];
                auto givenType = functionParameters->at(i);
                if (expectedType->isTypeArgument) {
                    expectedType = expectedType->typeBoundary;
                }
                if (!expectedType->isAssignableFrom(givenType)) {
                    filtered = true;
                    break;
                }
            }
            if (!filtered)
                filteredByFunctionParameters.push_back(calleeType);
        }

        if (filteredByFunctionParameters.size() == 1) {
            return filteredByFunctionParameters.at(0);
        }
        if (!filteredByFunctionParameters.empty()) {
            string possibilitiesStr;
            for (auto &type: filteredByFunctionParameters) {
                possibilitiesStr += type->toString() + ", ";
            }
            throw TypeExtractionException("multiple possible overloads are found, call is ambiguous. "
                                          "\npossibilities: [" + possibilitiesStr + "]");
        }
        throw TypeExtractionException("no possible overload is found");
    }

    TypeInfo *TypeHelper::getClassType(ClassDeclarationAstNode *class_) {

        auto allocationFunction = class_->allocationFunction;
        contextChain->push(allocationFunction->program, class_->name);
        auto classType = typeInfoRepository->findTypeByName(allocationFunction->program->contextObjectTypeName);

        for (auto &piece: allocationFunction->typeArguments) {
            auto name = piece.first;
            auto typeAst = piece.second;
            auto typeBoundary = typeOrError(typeAst);
            classType->addTypeArgument(name, typeBoundary);
        }

        allocationFunction->resolvedType = getFunctionTypeFromFunctionAst(allocationFunction);


        for (auto &stmt: class_->allocationFunction->program->statements) {
            if (stmt->type == StatementAstNode::TYPE_VARIABLE_DECLARATION) {
                auto name = stmt->variable->identifier;
                auto typeAst = stmt->variable->typeDescriptorAstNode;
                if (typeAst == nullptr) {
                    throw TypeExtractionException("class level variables must have explicit typing");
                }
                auto type = typeOrError(typeAst);
                classType->addProperty(name, type);
            } else if (stmt->type == StatementAstNode::TYPE_NAMED_FUNCTION) {
                auto function = stmt->namedFunction;
                auto functionType = getFunctionTypeFromFunctionAst(function);
                function->resolvedType = functionType;
                function->memoryIndex = classType->addProperty(function->name, functionType, true);
            }
        }

        contextChain->pop();
        return allocationFunction->resolvedType;
    }
}