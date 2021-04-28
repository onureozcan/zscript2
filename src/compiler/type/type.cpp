#include <compiler/type.h>

namespace zero {

    TypeInfo TypeInfo::STRING = TypeInfo(TYPE_LITERAL_STRING, 0);
    TypeInfo TypeInfo::INT = TypeInfo(TYPE_LITERAL_INT, 0);
    TypeInfo TypeInfo::BOOLEAN = TypeInfo(TYPE_LITERAL_BOOLEAN, 0);
    TypeInfo TypeInfo::DECIMAL = TypeInfo(TYPE_LITERAL_DECIMAL, 0);
    TypeInfo TypeInfo::ANY = TypeInfo(TYPE_LITERAL_ANY, 0);
    TypeInfo TypeInfo::T_VOID = TypeInfo(TYPE_LITERAL_VOID, 0);

    class TypeInfo::Impl {
    private:
        map<string, PropertyDescriptor *> propertiesMap;
        vector<TypeInfo *> typeParameters;
        int indexCounter = 0;
    public:

        unsigned int addProperty(string name, TypeInfo *pInfo) {
            if (propertiesMap.find(name) == propertiesMap.end()) {
                auto descriptor = new PropertyDescriptor();
                descriptor->name = pInfo->name;
                descriptor->typeInfo = pInfo;
                descriptor->index = indexCounter++;
                propertiesMap[name] = descriptor;
                return descriptor->index;
            } else
                throw runtime_error("property already defined : `" + name + "`");
        }

        PropertyDescriptor *getProperty(string name) {
            if (propertiesMap.find(name) != propertiesMap.end()) {
                return propertiesMap[name];
            }
            return nullptr;
        }

        void addParameter(TypeInfo *pInfo) {
            typeParameters.push_back(pInfo);
        }

        vector<TypeInfo *> getParameters() {
            return typeParameters;
        }

        int getPropertyCount() {
            return propertiesMap.size();
        }
    };

    TypeInfo::TypeInfo(string name, int isCallable, int isNative) {
        this->name = name;
        this->isCallable = isCallable;
        this->isNative = isNative;
        this->impl = new Impl();
    }

    unsigned int TypeInfo::addProperty(string name, TypeInfo *type) {
        return this->impl->addProperty(name, type);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getProperty(string name) {
        return this->impl->getProperty(name);
    }

    void TypeInfo::addParameter(TypeInfo *type) {
        return this->impl->addParameter(type);
    }

    int TypeInfo::isAssignableFrom(TypeInfo *other) {
        if (other == &TypeInfo::T_VOID) {
            // void cannot be assigned to anything
            return 0;
        }
        if (this == &TypeInfo::ANY) {
            // anything can be assigned to any
            return 1;
        }
        if (this == &TypeInfo::DECIMAL) {
            // int can be auto cast to decimal
            return other == &TypeInfo::INT || other == &TypeInfo::DECIMAL;
        }
        return other->name == this->name;
    }

    vector<TypeInfo *> TypeInfo::getParameters() {
        return impl->getParameters();
    }

    int TypeInfo::getPropertyCount() {
        return impl->getPropertyCount();
    }
}