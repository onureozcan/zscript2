#include <compiler/type.h>

namespace zero {

    TypeInfo TypeInfo::STRING = TypeInfo("String");
    TypeInfo TypeInfo::INT = TypeInfo("int");
    TypeInfo TypeInfo::DECIMAL = TypeInfo("decimal");
    TypeInfo TypeInfo::FUNCTION = TypeInfo("fun");
    TypeInfo TypeInfo::NATIVE_FUNCTION = TypeInfo("native");
    TypeInfo TypeInfo::ANY = TypeInfo("any");
    TypeInfo TypeInfo::_VOID = TypeInfo("void");
    TypeInfo TypeInfo::_NAN = TypeInfo("nan");

    class TypeInfo::Impl {
    private:
        map<string, PropertyDescriptor *> propertiesMap;
        vector<TypeInfo *> typeParameters;
        int indexCounter = 0;
    public:

        Impl() {

        }

        void addProperty(string name, TypeInfo *pInfo) {
            if (propertiesMap.find(name) == propertiesMap.end()) {
                auto descriptor = new PropertyDescriptor();
                descriptor->name = pInfo->name;
                descriptor->typeInfo = pInfo;
                descriptor->index = indexCounter++;
                propertiesMap[name] = descriptor;
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
    };

    TypeInfo::TypeInfo(string name) {
        this->name = name;
        this->impl = new Impl();
    }

    void TypeInfo::addProperty(string name, TypeInfo *type) {
        this->impl->addProperty(name, type);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getProperty(string name) {
        return this->impl->getProperty(name);
    }

    void TypeInfo::addParameter(TypeInfo *type) {
        return this->impl->addParameter(type);
    }
}