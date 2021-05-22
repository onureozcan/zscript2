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
        vector<pair<string, TypeInfo *>> typeParameters;
        vector<pair<string, string>> immediates;
        int indexCounter = 0;
    public:

        unsigned int addProperty(const string &propertyName, TypeInfo *typeInfo) {
            if (propertiesMap.find(propertyName) == propertiesMap.end()) {
                auto descriptor = new PropertyDescriptor();
                descriptor->name = propertyName;
                descriptor->typeInfo = typeInfo;
                descriptor->index = indexCounter++;
                propertiesMap[propertyName] = descriptor;
                return descriptor->index;
            } else if (propertiesMap[propertyName]->typeInfo->name == typeInfo->name) {
                return propertiesMap[propertyName]->index;
            } else
                throw runtime_error("property already defined with a different type : `" + propertyName + "`:`" +
                                    propertiesMap[propertyName]->typeInfo->name + "`");
        }

        PropertyDescriptor *getProperty(const string &propertyName) {
            if (propertiesMap.find(propertyName) != propertiesMap.end()) {
                return propertiesMap[propertyName];
            }
            return nullptr;
        }

        void addParameter(const string &ident, TypeInfo *pInfo) {
            typeParameters.push_back({ident, pInfo});
        }

        vector<pair<string, TypeInfo *>> getParameters() {
            return typeParameters;
        }

        map<string, PropertyDescriptor *> getProperties() {
            return propertiesMap;
        }

        int getPropertyCount() {
            return propertiesMap.size();
        }

        void removeProperty(const string &propertyName) {
            propertiesMap.erase(propertyName);
        }

        unsigned int addImmediate(const string &immediateData, TypeInfo *typeInfo) {
            auto immediateName = "$" + typeInfo->name + "__" + immediateData;
            immediates.push_back({immediateName, immediateData});
            return addProperty(immediateName, typeInfo);
        }

        PropertyDescriptor *getImmediate(const string &immediateData, TypeInfo *typeInfo) {
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

        string toString() {
            if (typeParameters.empty())
                return "";
            string parametersStr = "<";
            for (auto &param: typeParameters) {
                parametersStr += param.second->toString() + ",";
            }
            parametersStr += ">";
            return parametersStr;
        }
    };

    TypeInfo::TypeInfo(string name, int isCallable, int isNative, int isTypeParam) {
        this->name = std::move(name);
        this->isCallable = isCallable;
        this->isNative = isNative;
        this->isTypeParam = isTypeParam;
        this->impl = new Impl();
    }

    unsigned int TypeInfo::addProperty(const string &propertyName, TypeInfo *type) {
        return this->impl->addProperty(propertyName, type);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getProperty(const string &propertyName) {
        return this->impl->getProperty(propertyName);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getImmediate(const string &immediateName, TypeInfo *type) {
        return this->impl->getImmediate(immediateName, type);
    }

    void TypeInfo::addParameter(const string &parameterIdent, TypeInfo *type) {
        return this->impl->addParameter(parameterIdent, type);
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

    vector<pair<string, TypeInfo *>> TypeInfo::getParameters() {
        return impl->getParameters();
    }

    int TypeInfo::getPropertyCount() {
        return impl->getPropertyCount();
    }

    void TypeInfo::removeProperty(const string &propertyName) {
        return impl->removeProperty(propertyName);
    }

    unsigned int TypeInfo::addImmediate(const string &propertyName, TypeInfo *typeInfo) {
        return impl->addImmediate(propertyName, typeInfo);
    }

    vector<pair<string, string>> TypeInfo::getImmediateProperties() {
        return impl->getImmediateProperties();
    }

    void TypeInfo::clonePropertiesFrom(TypeInfo *other) {
        impl->clonePropertiesFrom(other);
    }

    string TypeInfo::toString() {
        return name + "" + impl->toString();
    }

    map<string, TypeInfo::PropertyDescriptor *> TypeInfo::getProperties() {
        return impl->getProperties();
    }
}