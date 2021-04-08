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
        int indexCounter = 0;
    public:
        void addProperty(string name, TypeInfo *pInfo) {
            auto descriptor = new PropertyDescriptor();
            descriptor->name = pInfo->name;
            descriptor->typeInfo = pInfo;
            descriptor->index = indexCounter++;
            propertiesMap[name] = descriptor;
        }

        PropertyDescriptor *getProperty(string name) {
            if (propertiesMap.find(name) != propertiesMap.end()) {
                return propertiesMap[name];
            }
            return nullptr;
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
}