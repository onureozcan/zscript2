#include <string>
#include <map>

#include <compiler/op.h>
#include <stdexcept>

using namespace std;

namespace zero {

    map<string, Operator *> operatorMap;

    string getMapKey(const string &name, int numberOfOperands) { return name + "$" + to_string(numberOfOperands); }

    Operator Operator::ADD = Operator("+", 2);
    Operator Operator::SUB = Operator("-", 2);
    Operator Operator::DIV = Operator("/", 2);
    Operator Operator::MUL = Operator("*", 2);
    Operator Operator::MOD = Operator("%", 2);

    Operator Operator::NEG = Operator("-", 1);

    Operator Operator::CMP_E = Operator("==", 2);
    Operator Operator::CMP_NE = Operator("!=", 2);
    Operator Operator::LT = Operator("<", 2);
    Operator Operator::LTE = Operator("<=", 2);
    Operator Operator::GT = Operator(">", 2);
    Operator Operator::GTE = Operator(">=", 2);

    Operator Operator::NOT = Operator("!", 1);

    Operator Operator::CALL = Operator("call", -1);
    Operator Operator::DOT = Operator(".", 2);

    Operator Operator::ASSIGN = Operator("=", 2);

    Operator::Operator(string name, int numberOfOperands) {
        this->name = name;
        this->numberOfOperands = numberOfOperands;
        operatorMap.insert({name + "$" + to_string(numberOfOperands), this});
    }

    Operator *Operator::getBy(string name, int numberOfOperands) {
        string key = getMapKey(name, numberOfOperands);
        if (operatorMap.find(key) != operatorMap.end())
            return operatorMap[getMapKey(name, numberOfOperands)];
        return nullptr;
    }

    string Operator::getReturnType(Operator *op, string type1, string type2) {
        if (op == &ASSIGN) {
            if (strcmp(type1.c_str(), type2.c_str()) == 0) {
                return type1;
            }
        }
        throw runtime_error("operator `" + op->name + "` does not work on `" + type1 + "` and `" + type2 + "`");
    }

    string Operator::getReturnType(Operator *op, string type1) {

        throw runtime_error("operator `" + op->name + "` does not work on `" + type1 + "`");
    }
}