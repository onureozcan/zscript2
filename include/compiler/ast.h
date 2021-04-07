#pragma once

#include <vector>

#include <compiler/type.h>
#include <compiler/op.h>

#include "ZParser.h"

using namespace std;

namespace zero {

    class BaseAstNode {
    public:
        string fileName;
        int line;
        int pos;

        virtual string toString() {
            return string("ast node at " + fileName + to_string(line) + "," + to_string(pos));
        }
    };

    class ExpressionAstNode : public BaseAstNode {
    public:
        TypeInfo *typeInfo;
        int expressionType;

        static int TYPE_ATOMIC;
        static int TYPE_UNARY;
        static int TYPE_BINARY;
        static int TYPE_FUNCTION_CALL;

        static ExpressionAstNode *from(ZParser::ExpressionContext *expressionContext, string fileName);
    };

    class BinaryExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *left, *right;
        Operator *op;
    };

    class PrefixExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *right;
        Operator *op;
    };

    class FunctionCallExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *left;
        vector<ExpressionAstNode *> *args;
    };

    class AtomicExpressionAstNode : public ExpressionAstNode {
    public:
        string data;
        int atomicType;

        static int TYPE_IDENTIFIER;
        static int TYPE_INT;
        static int TYPE_DECIMAL;
        static int TYPE_STRING;
        static int TYPE_FUNCTION;
    };

    class VariableAstNode : public BaseAstNode {
    public:
        string identifier;
        ExpressionAstNode *initialValue;

        static VariableAstNode *from(ZParser::VariableDeclarationContext *variableDeclarationContext, string fileName);

        string toString() override;
    };

    class StatementAstNode : public BaseAstNode {
    public:
        ExpressionAstNode *expression;
        VariableAstNode *variable;
        int type;

        static int TYPE_EXPRESSION;
        static int TYPE_VARIABLE_DECLARATION;

        static StatementAstNode *from(ZParser::StatementContext *statementContext, string fileName);

        string toString() override;
    };


    class ProgramAstNode : public BaseAstNode {
    public:
        vector<StatementAstNode *> *statements;

        static ProgramAstNode *from(ZParser::ProgramContext *programContext, string fileName);

        string toString() override;
    };

    class FunctionAstNode : public AtomicExpressionAstNode {
    public:
        string identifier;
        ProgramAstNode *program;
        vector<string> *arguments;

        static FunctionAstNode *from(ZParser::FunctionContext *functionContext, string fileName);

        string toString() override;
    };
}