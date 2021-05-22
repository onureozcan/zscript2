#include <compiler/type_meta.h>

namespace zero {

    TypeMetadataRepository* TypeMetadataRepository::instance = nullptr;

    class TypeMetadataRepository::Impl {
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

    TypeMetadataRepository::TypeMetadataRepository() {
        this->impl = new Impl();
    }

    void TypeMetadataRepository::registerType(TypeInfo *type) {
        impl->registerType(type);
    }

    TypeInfo *TypeMetadataRepository::findTypeByName(const string& name) {
        return impl->findTypeByName(name);
    }

    TypeMetadataRepository *TypeMetadataRepository::getInstance() {
        if (instance == nullptr) instance = new TypeMetadataRepository();
        return instance;
    }
}