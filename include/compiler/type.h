#pragma once

#include <string>

#include <compiler/ast.h>

using namespace std;

namespace zero {
    class TypeInfo {
        class Impl;
    public:

        class PropertyDescriptor {
        public:
            string name;
            TypeInfo *typeInfo;
            int index;
        };

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

        void addParameter(TypeInfo *type);

        void addProperty(string name, TypeInfo *type);

        PropertyDescriptor *getProperty(string name);

    private:
        Impl *impl;
    };
}