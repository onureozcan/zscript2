#pragma once

#include <string>
#include <vector>

using namespace std;

namespace zero {

    class Operator {
    public:
        string name;
        int numberOfOperands;

        // numeric binary
        static Operator ADD;
        static Operator SUB;
        static Operator MUL;
        static Operator DIV;
        static Operator MOD;

        // numeric unary
        static Operator NEG;

        // logical binary
        static Operator CMP_E;
        static Operator CMP_NE;
        static Operator LT;
        static Operator LTE;
        static Operator GT;
        static Operator GTE;
        static Operator AND;
        static Operator OR;

        // logical unary
        static Operator NOT;

        // language specific
        static Operator CALL;
        static Operator DOT;
        static Operator ASSIGN;

        Operator(string name, int numberOfOperands);

        static Operator *getBy(string name, int numberOfOperands);

        static string getReturnType(Operator *op, string type1, string type2);

        static string getReturnType(Operator *op, string operand1);
    };
}