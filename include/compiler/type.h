#pragma once

#include <string>
#include <vector>
#include <map>

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

        TypeInfo* typeBoundary = nullptr;
        int isTypeArgument;

        int isCallable;
        int isNative;

        static TypeInfo INT;
        static TypeInfo DECIMAL;
        static TypeInfo STRING;
        static TypeInfo BOOLEAN;
        static TypeInfo ANY;
        static TypeInfo T_VOID;

        explicit TypeInfo(string name, int isCallable, int isNative = 0, int isTypeArgument = 0);

        TypeInfo *resolveGenericType(const map<string, TypeInfo *> *passedTypeParametersMap);

        void addTypeArgument(const string& typeArgName, TypeInfo *type);

        vector<pair<string, TypeInfo *>> getTypeArguments();

        vector<TypeInfo*> getFunctionArguments();

        unsigned int addProperty(const string& propertyName, TypeInfo *type);

        int isAssignableFrom(TypeInfo *other);

        int getPropertyCount();

        PropertyDescriptor *getProperty(const string& propertyName);

        void removeProperty(const string& basicString);

        unsigned int addImmediate(const string& basicString, TypeInfo *pInfo);

        PropertyDescriptor *getImmediate(const string& immediateName, TypeInfo *type);

        vector<pair<string, string>> getImmediateProperties();

        map<string,PropertyDescriptor*> getProperties();

        void clonePropertiesFrom(TypeInfo *other);

        void addFunctionArgument(TypeInfo *argumentType);

        string toString();

    private:
        Impl *impl;
    };
}