#include <compiler/type.h>
#include <map>

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
        vector<pair<string, string>> immediates;
        int indexCounter = 0;
    public:

        unsigned int addProperty(string name, TypeInfo *typeInfo) {
            if (propertiesMap.find(name) == propertiesMap.end()) {
                auto descriptor = new PropertyDescriptor();
                descriptor->name = name;
                descriptor->typeInfo = typeInfo;
                descriptor->index = indexCounter++;
                propertiesMap[name] = descriptor;
                return descriptor->index;
            } else if (propertiesMap[name]->typeInfo->name == typeInfo->name) {
                return propertiesMap[name]->index;
            } else
                throw runtime_error("property already defined with a different type : `" + name + "`:`" +
                                    propertiesMap[name]->typeInfo->name + "`");
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

        void removeProperty(string propertyName) {
            propertiesMap.erase(propertyName);
        }

        unsigned int addImmediate(string immediateData, TypeInfo *typeInfo) {
            auto immediateName = "$" + typeInfo->name + "__" + immediateData;
            immediates.push_back({immediateName, immediateData});
            return addProperty(immediateName, typeInfo);
        }

        PropertyDescriptor *getImmediate(string immediateData, TypeInfo *typeInfo) {
            auto immediateName = "$" + typeInfo->name + "__" + immediateData;
            return getProperty(immediateName);
        }

        vector<pair<string, string>> getImmediateProperties() {
            return immediates;
        }

        void clonePropertiesFrom(TypeInfo *other) {
            this->immediates = vector<pair<string, string>>();
            this->propertiesMap = map<string, PropertyDescriptor *>(other->impl->propertiesMap);
            this->indexCounter = other->impl->indexCounter;
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

    TypeInfo::PropertyDescriptor *TypeInfo::getImmediate(string name, TypeInfo *type) {
        return this->impl->getImmediate(name, type);
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

    void TypeInfo::removeProperty(string propertyName) {
        return impl->removeProperty(propertyName);
    }

    unsigned int TypeInfo::addImmediate(string propertyName, TypeInfo *typeInfo) {
        return impl->addImmediate(propertyName, typeInfo);
    }

    vector<pair<string, string>> TypeInfo::getImmediateProperties() {
        return impl->getImmediateProperties();
    }

    void TypeInfo::clonePropertiesFrom(TypeInfo *other) {
        impl->clonePropertiesFrom(other);
    }
}