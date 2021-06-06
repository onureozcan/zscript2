#include <compiler/type.h>
#include <map>

namespace zero {

    TypeInfo TypeInfo::NULL_ = TypeInfo(TYPE_LITERAL_NULL, 0);
    TypeInfo TypeInfo::STRING = TypeInfo(TYPE_LITERAL_STRING, 0);
    TypeInfo TypeInfo::INT = TypeInfo(TYPE_LITERAL_INT, 0);
    TypeInfo TypeInfo::BOOLEAN = TypeInfo(TYPE_LITERAL_BOOLEAN, 0);
    TypeInfo TypeInfo::DECIMAL = TypeInfo(TYPE_LITERAL_DECIMAL, 0);
    TypeInfo TypeInfo::ANY = TypeInfo(TYPE_LITERAL_ANY, 0);
    TypeInfo TypeInfo::T_VOID = TypeInfo(TYPE_LITERAL_VOID, 0);

    int TypeInfo::PropertyDescriptor::addOverload(TypeInfo *type, int index) {
        typeInfoIndexMap.push_back({type, index});
        return index;
    }

    TypeInfo::PropertyDescriptor::OverloadInfo TypeInfo::PropertyDescriptor::firstOverload() {
        auto info = OverloadInfo();
        pair<TypeInfo *, int> &pair = typeInfoIndexMap.front();
        info.type = pair.first;
        info.index = pair.second;
        return info;
    }

    vector<TypeInfo::PropertyDescriptor::OverloadInfo> TypeInfo::PropertyDescriptor::allOverloads() {
        auto ret = vector<TypeInfo::PropertyDescriptor::OverloadInfo>();
        for (auto overload: typeInfoIndexMap) {
            ret.push_back({
                                  overload.first,
                                  overload.second
                          });
        }
        return ret;
    }

    int TypeInfo::PropertyDescriptor::indexOfOverloadOrMinusOne(TypeInfo *type) {
        for (auto pair : typeInfoIndexMap) {
            auto overloadType = pair.first;
            if (overloadType->equals(type)) {
                return pair.second;
            }
        }
        return -1;
    }

    class TypeInfo::Impl {
    private:
        TypeInfo *publicSelf;
        map<string, PropertyDescriptor *> propertiesMap;
        vector<pair<string, TypeInfo *>> typeArguments;
        vector<TypeInfo *> functionArguments;
        vector<pair<string, string>> immediates;
        int indexCounter = 0;
    public:

        explicit Impl(TypeInfo *publicSelf) {
            this->publicSelf = publicSelf;
        }

        unsigned int addProperty(const string &propertyName, TypeInfo *typeInfo, int overloadable = false) {
            if (propertiesMap.find(propertyName) == propertiesMap.end()) {
                auto descriptor = new PropertyDescriptor();
                descriptor->name = propertyName;
                descriptor->addOverload(typeInfo, indexCounter++);
                propertiesMap[propertyName] = descriptor;
                return descriptor->firstOverload().index;
            } else if (overloadable) {
                auto descriptor = propertiesMap[propertyName];
                int existingIndex = descriptor->indexOfOverloadOrMinusOne(typeInfo);
                if (existingIndex != -1) {
                    throw runtime_error("overload of the same kind was already defined!");
                } else {
                    descriptor->addOverload(typeInfo, indexCounter++);
                    return indexCounter - 1;
                }
            } else {
                auto descriptor = propertiesMap[propertyName];
                auto existingType = descriptor->firstOverload().type;
                if (existingType->equals(typeInfo)) {
                    return descriptor->firstOverload().index;
                }
            }
            throw runtime_error("non-overloadable property already defined with a different type : " + propertyName);
        }

        PropertyDescriptor *getProperty(const string &propertyName) {
            if (propertiesMap.find(propertyName) != propertiesMap.end()) {
                return propertiesMap[propertyName];
            }
            return nullptr;
        }

        void addTypeArgument(const string &ident, TypeInfo *pInfo) {
            typeArguments.push_back({ident, pInfo});
        }

        vector<pair<string, TypeInfo *>> getTypeArguments() {
            return typeArguments;
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

        static TypeInfo *
        resolveGenericType(TypeInfo *genericType, const map<string, TypeInfo *> *passedTypeParameters) {
            // non-generic
            if (passedTypeParameters->empty()) return genericType;
            if (genericType->isTypeArgument) {
                return passedTypeParameters->find(genericType->name)->second;
            } else {
                auto clone = new TypeInfo(genericType->name, genericType->isCallable, genericType->isNative);
                // resolve recursively
                auto properties = genericType->impl->propertiesMap;
                auto typeArguments = genericType->impl->typeArguments;
                auto functionArguments = genericType->impl->functionArguments;
                auto immediateProperties = genericType->getImmediateProperties();
                for (const auto &actualParam : typeArguments) {
                    clone->addTypeArgument(actualParam.first, actualParam.second);
                }
                for (const auto &actualProp: properties) {
                    clone->impl->typeArguments = typeArguments;
                }
                for (const auto &actualArg: functionArguments) {
                    auto resolvedArg = resolveGenericType(actualArg, passedTypeParameters);
                    clone->addFunctionArgument(resolvedArg);
                }
                clone->impl->immediates = genericType->impl->immediates;
                return clone;
            }
        }

        void addFunctionArgument(TypeInfo *argumentType) {
            functionArguments.push_back(argumentType);
        }

        vector<TypeInfo *> getFunctionArguments() {
            return functionArguments;
        }

        bool equals(TypeInfo *other) {
            return other->toString() == this->toString();
        }

        string toString() {
            string parametersStr;
            string functionArgumentsStr;
            if (!typeArguments.empty()) {
                parametersStr = "<";
                for (auto &param: typeArguments) {
                    parametersStr += param.second->toString() + ",";
                }
                parametersStr += ">";
            }
            if (!functionArguments.empty()) {
                functionArgumentsStr = "(";
                for (auto &arg: functionArguments) {
                    functionArgumentsStr += arg->toString() + ",";
                }
                functionArgumentsStr += ")";
            }
            return publicSelf->name + parametersStr + functionArgumentsStr;
        }
    };

    TypeInfo::TypeInfo(string name, int isCallable, int isNative, int isTypeArgument) {
        this->name = std::move(name);
        this->isCallable = isCallable;
        this->isNative = isNative;
        this->isTypeArgument = isTypeArgument;
        this->impl = new Impl(this);
    }

    unsigned int TypeInfo::addProperty(const string &propertyName, TypeInfo *type, int overloadable) {
        return this->impl->addProperty(propertyName, type, overloadable);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getProperty(const string &propertyName) {
        return this->impl->getProperty(propertyName);
    }

    TypeInfo::PropertyDescriptor *TypeInfo::getImmediate(const string &immediateName, TypeInfo *type) {
        return this->impl->getImmediate(immediateName, type);
    }

    void TypeInfo::addTypeArgument(const string &typeArgName, TypeInfo *type) {
        return this->impl->addTypeArgument(typeArgName, type);
    }

    int TypeInfo::isAssignableFrom(TypeInfo *other) {
        auto t1 = this;
        auto t2 = other;
        if (this->isTypeArgument) {
            t1 = this->typeBoundary;
        }
        if (other->isTypeArgument) {
            t2 = other->typeBoundary;
        }

        if (t1->equals(&TypeInfo::NULL_)) {
            // nothing can be assigned to null
            return false;
        }

        if (t2 == &TypeInfo::T_VOID) {
            // void cannot be assigned to anything
            return 0;
        }
        if (t1 == &TypeInfo::ANY) {
            // anything can be assigned to any
            return 1;
        }
        if (t1 == &TypeInfo::DECIMAL) {
            // int can be auto cast to decimal
            return t2 == &TypeInfo::INT || t2 == &TypeInfo::DECIMAL;
        }

        if (t2->equals(&TypeInfo::NULL_)) {
            // null cannot be assigned tp decimal or int
            return !t1->equals(&TypeInfo::DECIMAL) && !t1->equals(&TypeInfo::INT);
        }

        // check names
        if (t1->name == t2->name) {
            // check type arguments
            auto thisTypeArgs = t1->getTypeArguments();
            auto otherTypeArgs = t2->getTypeArguments();
            if (thisTypeArgs.size() == otherTypeArgs.size()) {
                for (int i = 0; i < otherTypeArgs.size(); i++) {
                    auto thisArg = thisTypeArgs.at(i);
                    auto otherArg = otherTypeArgs.at(i);
                    if (!thisArg.second->isAssignableFrom(otherArg.second)) {
                        goto end;
                    }
                }
            } else goto end;
            // check function arguments
            auto thisFncArgs = t1->getFunctionArguments();
            auto otherFncArgs = t2->getFunctionArguments();
            if (thisFncArgs.size() == otherFncArgs.size()) {
                for (int i = 0; i < thisFncArgs.size(); i++) {
                    auto thisArg = thisFncArgs.at(i);
                    auto otherArg = otherFncArgs.at(i);
                    if (!thisArg->isAssignableFrom(otherArg)) {
                        goto end;
                    }
                }
            } else goto end;
        }

        end:
        return t2->equals(t1);
    }

    vector<pair<string, TypeInfo *>> TypeInfo::getTypeArguments() {
        return impl->getTypeArguments();
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
        return impl->toString();
    }

    map<string, TypeInfo::PropertyDescriptor *> TypeInfo::getProperties() {
        return impl->getProperties();
    }

    TypeInfo *TypeInfo::resolveGenericType(const map<string, TypeInfo *> *passedTypeParametersMap) {
        return Impl::resolveGenericType(this, passedTypeParametersMap);
    }

    void TypeInfo::addFunctionArgument(TypeInfo *argumentType) {
        return impl->addFunctionArgument(argumentType);
    }

    vector<TypeInfo *> TypeInfo::getFunctionArguments() {
        return impl->getFunctionArguments();
    }

    bool TypeInfo::equals(TypeInfo *other) {
        return impl->equals(other);
    }
}