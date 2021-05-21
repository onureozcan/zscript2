#include <compiler/type_meta.h>

namespace zero {

    TypeMetadataRepository* TypeMetadataRepository::instance = nullptr;

    class TypeMetadataRepository::Impl {
    private:
        map<string, TypeInfo *> typeMap;

        static string getSearchKey(const string& name, int parameterCount) {
            return name + "<" + to_string(parameterCount) + ">";
        }

        static string getSearchKey(TypeInfo *type) {
            return type->name + "<" + to_string(type->getParameters().size()) + ">";
        }

    public:
        Impl() {
            registerType(&TypeInfo::STRING);
            registerType(&TypeInfo::INT);
            registerType(&TypeInfo::BOOLEAN);
            registerType(&TypeInfo::DECIMAL);
            registerType(&TypeInfo::ANY);
            registerType(&TypeInfo::T_VOID);
        }

        TypeInfo *findTypeByName(const string& name, int paramCount = 0) {
            auto searchKey = getSearchKey(name, paramCount);
            if (typeMap.find(searchKey) != typeMap.end())
                return typeMap[searchKey];
            else return nullptr;
        }

        void registerType(TypeInfo *type) {
            typeMap[getSearchKey(type)] = type;
        }
    };

    TypeMetadataRepository::TypeMetadataRepository() {
        this->impl = new Impl();
    }

    void TypeMetadataRepository::registerType(TypeInfo *type) {
        impl->registerType(type);
    }

    TypeInfo *TypeMetadataRepository::findTypeByName(const string& name, int parameterCount) {
        return impl->findTypeByName(name, parameterCount);
    }

    TypeMetadataRepository *TypeMetadataRepository::getInstance() {
        if (instance == nullptr) instance = new TypeMetadataRepository();
        return instance;
    }
}