#pragma once

#include <string>
#include <vector>

#define TYPE_LITERAL_STRING "String"
#define TYPE_LITERAL_INT "int"
#define TYPE_LITERAL_BOOLEAN "boolean"
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
        int isNative;

        static TypeInfo INT;
        static TypeInfo DECIMAL;
        static TypeInfo STRING;
        static TypeInfo BOOLEAN;
        static TypeInfo ANY;
        static TypeInfo T_VOID;

        explicit TypeInfo(string name, int isCallable, int isNative = 0);

        void addParameter(TypeInfo *type);

        vector<TypeInfo *> getParameters();

        unsigned int addProperty(string name, TypeInfo *type);

        int isAssignableFrom(TypeInfo *other);

        int getPropertyCount();

        PropertyDescriptor *getProperty(string name);

        void removeProperty(string basicString);

        unsigned int addImmediate(string basicString, TypeInfo *pInfo);

        PropertyDescriptor *getImmediate(string name, TypeInfo *type);

        vector<pair<string, string>> getImmediateProperties();

        void clonePropertiesFrom(TypeInfo *other);

    private:
        Impl *impl;
    };
}