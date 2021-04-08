#pragma once

#include <string>

#include <compiler/ast.h>

using namespace std;

namespace zero {
    class TypeInfo {
        class Impl;

        class PropertyDescriptor {
        public:
            string name;
            TypeInfo *typeInfo;
            int index;
        };

    public:
        string name;

        static TypeInfo INT;
        static TypeInfo DECIMAL;
        static TypeInfo STRING;
        static TypeInfo FUNCTION;
        static TypeInfo NATIVE_FUNCTION;
        static TypeInfo ANY;
        static TypeInfo _VOID;
        static TypeInfo _NAN;

        explicit TypeInfo(string name);

        void addProperty(string name, TypeInfo *type);

        PropertyDescriptor *getProperty(string name);

    private:
        Impl *impl;
    };

    class TypeMetadataRepository {
        class Impl;

    public:
        void registerType(string name, TypeInfo *type);

        TypeInfo *findTypeByName(string name);

        TypeMetadataRepository();

    private:
        Impl *impl;
    };

    class TypeMetadataExtractor {
        class Impl;

    public:
        void extractAndRegister(BaseAstNode *ast);

        TypeMetadataExtractor();

    private:
        Impl *impl;
    };
}