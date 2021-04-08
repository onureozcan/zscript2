#include <compiler/type_meta.h>

namespace zero {

    class TypeMetadataRepository::Impl {
    private:
        map<string, TypeInfo *> typeMap;
    public:
        TypeInfo *findTypeByName(string name) {
            if (typeMap.find(name) != typeMap.end())
                return typeMap[name];
            else return nullptr;
        }

        void registerType(string name, TypeInfo *type) {
            typeMap[name] = type;
        }
    };

    TypeMetadataRepository::TypeMetadataRepository() {
        this->impl = new Impl();
    }

    void TypeMetadataRepository::registerType(string name, TypeInfo *type) {
        impl->registerType(name, type);
    }

    TypeInfo *TypeMetadataRepository::findTypeByName(string name) {
        return impl->findTypeByName(name);
    }
}