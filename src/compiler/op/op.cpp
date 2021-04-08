#include <string>
#include <map>

#include <compiler/op.h>

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
}