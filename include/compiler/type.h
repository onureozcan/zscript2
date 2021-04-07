#pragma once

#include <string>

using namespace std;

namespace zero {
    class TypeInfo {
    public:
        string name;

        static TypeInfo INT;
        static TypeInfo DECIMAL;
        static TypeInfo STRING;
        static TypeInfo FUNCTION;
        static TypeInfo OBJECT;
        static TypeInfo ANY;
        static TypeInfo _VOID;
        static TypeInfo _NAN;

        TypeInfo(string name);
    };
}