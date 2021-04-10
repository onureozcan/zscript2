#pragma once

#include <vector>

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
        int expressionType;

        string typeName = "?";

        int memoryIndex; // memory index in the current context
        int memoryDepth; // how many contexts i need to go up to find this property - only relevant for direct access

        int isLvalue;

        static const int TYPE_ATOMIC = 0;
        static const int TYPE_UNARY = 1;
        static const int TYPE_BINARY = 2;
        static const int TYPE_FUNCTION_CALL = 3;

        static ExpressionAstNode *from(ZParser::ExpressionContext *expressionContext, string fileName);

        string toString() override;
    };

    class BinaryExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *left, *right;
        string opName;
    };

    class PrefixExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *right;
        string opName;
    };

    class FunctionCallExpressionAstNode : public ExpressionAstNode {
    public:
        ExpressionAstNode *left;
        vector<ExpressionAstNode *> *params;
    };

    class AtomicExpressionAstNode : public ExpressionAstNode {
    public:
        string data;
        int atomicType;

        static const int TYPE_IDENTIFIER = 0;
        static const int TYPE_INT = 1;
        static const int TYPE_DECIMAL = 2;
        static const int TYPE_STRING = 3;
        static const int TYPE_FUNCTION = 4;
    };

    class VariableAstNode : public BaseAstNode {
    public:
        string identifier;
        string typeName;
        int hasExplicitTypeInfo;
        ExpressionAstNode *initialValue;

        static VariableAstNode *from(ZParser::VariableDeclarationContext *variableDeclarationContext, string fileName);

        string toString() override;
    };

    class StatementAstNode : public BaseAstNode {
    public:
        ExpressionAstNode *expression;
        VariableAstNode *variable;
        int type;

        static const int TYPE_EXPRESSION = 0;
        static const int TYPE_VARIABLE_DECLARATION = 1;
        static const int TYPE_RETURN = 2;

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
        vector<pair<string, string>> *arguments;
        string returnTypeName;

        static FunctionAstNode *from(ZParser::FunctionContext *functionContext, string fileName);
    };
}