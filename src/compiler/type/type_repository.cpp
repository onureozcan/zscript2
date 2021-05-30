#include <compiler/type_meta.h>

namespace zero {

    TypeInfoRepository* TypeInfoRepository::instance = nullptr;

    class TypeInfoRepository::Impl {
    private:
        map<string, TypeInfo *> typeMap;

    public:
        Impl() {
            registerType(&TypeInfo::STRING);
            registerType(&TypeInfo::INT);
            registerType(&TypeInfo::BOOLEAN);
            registerType(&TypeInfo::DECIMAL);
            registerType(&TypeInfo::ANY);
            registerType(&TypeInfo::T_VOID);
        }

        TypeInfo *findTypeByName(const string& name) {
            if (typeMap.find(name) != typeMap.end())
                return typeMap[name];
            else return nullptr;
        }

        void registerType(TypeInfo *type) {
            typeMap[type->name] = type;
        }
    };

    TypeInfoRepository::TypeInfoRepository() {
        this->impl = new Impl();
    }

    void TypeInfoRepository::registerType(TypeInfo *type) {
        impl->registerType(type);
    }

    TypeInfo *TypeInfoRepository::findTypeByName(const string& name) {
        return impl->findTypeByName(name);
    }

    TypeInfoRepository *TypeInfoRepository::getInstance() {
        if (instance == nullptr) instance = new TypeInfoRepository();
        return instance;
    }
}