#pragma once

#include <string>

#include <compiler/ast.h>
#include <compiler/type.h>

using namespace std;

namespace zero {
    class TypeMetadataRepository {
        class Impl;

    public:

        static TypeMetadataRepository * getInstance();

        void registerType(TypeInfo *type);

        TypeInfo *findTypeByName(const string& name, int parameterCount = 0);

    private:
        TypeMetadataRepository();

        static TypeMetadataRepository* instance;

        Impl *impl;
    };

    class TypeMetadataExtractor {
        class Impl;

    public:
        void extractAndRegister(ProgramAstNode *program);

        explicit TypeMetadataExtractor();

    private:
        Impl *impl;
    };
}