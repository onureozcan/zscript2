#include <string>
#include <map>
#include <stdexcept>

#include <compiler/op.h>
#include <compiler/type.h>

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
    Operator Operator::AND = Operator("&&", 2);
    Operator Operator::OR = Operator("||", 2);

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

    TypeInfo *Operator::getReturnType(Operator *op, TypeInfo *type1, TypeInfo *type2) {
        if (op == &ASSIGN) {
            if (type1->name == type2->name) {
                return &TypeInfo::T_VOID;
            }
        }
        auto isNumericOperator = op == &MUL || op == &DIV || op == &SUB || op == &ADD || op == &MOD;

        if (isNumericOperator) {
            if (type1->name == TypeInfo::DECIMAL.name) {
                if (type2->name == TypeInfo::DECIMAL.name || type2->name == TypeInfo::INT.name) {
                    return type1;
                }
            } else if (type1->name == TypeInfo::INT.name) {
                if (type2->name == TypeInfo::DECIMAL.name || type2->name == TypeInfo::INT.name) {
                    return type2;
                }
            }
        }

        if (op == &ADD) {
            // string addition case
            if (type1->name == TypeInfo::STRING.name && type2->name == type1->name) {
                return type1;
            }
        }

        auto isNumericComparisonOperator =
                op == &CMP_E || op == &CMP_NE || op == &LT || op == &LTE || op == &GT || op == &GTE;

        if (isNumericComparisonOperator) {
            if ((type1->name == TypeInfo::INT.name || type1->name == TypeInfo::DECIMAL.name)
                && (type2->name == TypeInfo::INT.name || type2->name == TypeInfo::DECIMAL.name)) {
                return &TypeInfo::BOOLEAN;
            }
        }

        if (op == &AND || op == &OR) {
            if (type1->name == type2->name && type1->name == TypeInfo::BOOLEAN.name) {
                return &TypeInfo::BOOLEAN;
            }
        }

        // try type parameters before throwing the error
        if (type1->isTypeParam || type2->isTypeParam) {
            TypeInfo *type1TypeBoundary, *type2TypeBoundary;
            type1TypeBoundary = type1->typeBoundary == nullptr ? type1 : type1->typeBoundary;
            type2TypeBoundary = type2->typeBoundary == nullptr ? type2 : type2->typeBoundary;
            TypeInfo *ret = getReturnType(op, type1TypeBoundary, type2TypeBoundary);
            if (ret == type1TypeBoundary) {
                return type1;
            } else if (ret == type2TypeBoundary) {
                return type2;
            } else {
                return ret;
            }
        }

        throw runtime_error(
                "operator `" + op->name + "` does not work on `" + type1->name + "` and `" + type2->name + "`");
    }

    TypeInfo *Operator::getReturnType(Operator *op, TypeInfo *type1) {
        if (op == &Operator::NEG) {
            if (type1->name == TypeInfo::DECIMAL.name || type1->name == TypeInfo::INT.name) {
                return type1;
            }
        } else if (op == &Operator::NOT) {
            if (type1->name == TypeInfo::BOOLEAN.name) {
                return type1;
            }
        }

        // try type boundaries before throwing error
        if (type1->isTypeParam) {
            TypeInfo *ret = getReturnType(op, type1->typeBoundary);
            if (ret == type1->typeBoundary) {
                return type1;
            }
            return ret;
        }

        throw runtime_error("operator `" + op->name + "` does not work on `" + type1->name + "`");
    }
}