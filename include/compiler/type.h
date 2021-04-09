#pragma once

#include <string>
#include <compiler/ast.h>

#define TYPE_LITERAL_STRING "String"
#define TYPE_LITERAL_INT "int"
#define TYPE_LITERAL_DECIMAL "decimal"
#define TYPE_LITERAL_ANY "Any"
#define TYPE_LITERAL_VOID "Void"

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
        int isCallable;

        static TypeInfo INT;
        static TypeInfo DECIMAL;
        static TypeInfo STRING;
        static TypeInfo ANY;
        static TypeInfo T_VOID;

        explicit TypeInfo(string name, int isCallable);

        void addParameter(TypeInfo *type);

        vector<TypeInfo*> getParameters();

        void addProperty(string name, TypeInfo *type);

        int isAssignableFrom(TypeInfo* other);

        PropertyDescriptor *getProperty(string name);

    private:
        Impl *impl;
    };
}