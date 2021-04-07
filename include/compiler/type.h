#pragma once

#include <string>

using namespace std;

namespace zero {
    class TypeInfo {
    public:
        const string name;

        static TypeInfo NUMERIC;
        static TypeInfo STRING;
        static TypeInfo FUNCTION;
        static TypeInfo OBJECT;
        static TypeInfo ANY;
        static TypeInfo _VOID;
        static TypeInfo _NAN;

        TypeInfo(const string name);
    };
}