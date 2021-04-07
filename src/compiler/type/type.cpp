#include <compiler/type.h>

namespace zero {

    TypeInfo TypeInfo::STRING = TypeInfo("String");
    TypeInfo TypeInfo::INT = TypeInfo("int");
    TypeInfo TypeInfo::DECIMAL = TypeInfo("decimal");
    TypeInfo TypeInfo::FUNCTION = TypeInfo("fun");
    TypeInfo TypeInfo::OBJECT = TypeInfo("obj");
    TypeInfo TypeInfo::ANY = TypeInfo("any");
    TypeInfo TypeInfo::_VOID = TypeInfo("void");
    TypeInfo TypeInfo::_NAN = TypeInfo("nan");

    TypeInfo::TypeInfo(string name) {
        this->name = name;
    }
}