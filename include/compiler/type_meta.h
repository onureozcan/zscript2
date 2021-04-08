#pragma once

#include <string>

#include <compiler/ast.h>
#include <compiler/type.h>

using namespace std;

namespace zero {
    class TypeMetadataRepository {
        class Impl;

    public:
        void registerType(TypeInfo *type);

        TypeInfo *findTypeByName(string name);

        TypeMetadataRepository();

    private:
        Impl *impl;
    };

    class TypeMetadataExtractor {
        class Impl;

    public:
        void extractAndRegister(FunctionAstNode *function);

        explicit TypeMetadataExtractor(TypeMetadataRepository* repository);

    private:
        Impl *impl;
    };
}