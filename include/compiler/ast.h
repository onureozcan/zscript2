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

        unsigned int memoryIndex; // memory index in the current context
        unsigned int memoryDepth; // how many contexts i need to go up to find this property - only relevant for direct access

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
        static const int TYPE_BOOLEAN = 3;
        static const int TYPE_STRING = 4;
        static const int TYPE_FUNCTION = 5;
    };

    class VariableAstNode : public BaseAstNode {
    public:
        string identifier;
        string typeName;
        int hasExplicitTypeInfo;
        unsigned int memoryIndex;
        ExpressionAstNode *initialValue;

        static VariableAstNode *from(ZParser::VariableDeclarationContext *variableDeclarationContext, string fileName);

        string toString() override;
    };

    class IfStatementAstNode;
    class LoopAstNode;

    class StatementAstNode : public BaseAstNode {
    public:
        ExpressionAstNode *expression;
        VariableAstNode *variable;
        IfStatementAstNode* ifStatement;
        LoopAstNode* loop;
        int type;

        static const int TYPE_EXPRESSION = 0;
        static const int TYPE_VARIABLE_DECLARATION = 1;
        static const int TYPE_RETURN = 2;
        static const int TYPE_IF = 3;
        static const int TYPE_LOOP = 4;

        static StatementAstNode *from(ZParser::StatementContext *statementContext, string fileName);

        string toString() override;
    };

    class ProgramAstNode : public BaseAstNode {
    public:
        vector<StatementAstNode *> *statements;

        string contextObjectTypeName;

        static ProgramAstNode *from(ZParser::ProgramContext *programContext, string fileName);

        string toString() override;
    };

    class IfStatementAstNode: public BaseAstNode {
    public:
        ExpressionAstNode* expression;
        ProgramAstNode* program, *elseProgram;

        static IfStatementAstNode *from(ZParser::IfStatementContext* ifContext, string fileName);

        string toString() override;
    };

    class LoopAstNode: public BaseAstNode {
    public:
        VariableAstNode* loopVariable;
        ExpressionAstNode* loopIterationExpression,*loopConditionExpression;
        ProgramAstNode* program;

        static LoopAstNode *from(ZParser::ForLoopContext* forLoopContext, string fileName);

        string toString() override;
    };

    class FunctionAstNode : public AtomicExpressionAstNode {
    public:
        ProgramAstNode *program;
        vector<pair<string, string>> *arguments;
        string returnTypeName;
        // this one is important, let me explain:
        // if a function has a function definition inside, it is perfectly legal for the child function to access variables in the "upper" scope
        // in that case, we cannot simply destroy the parent function context. it is a well-known pattern of memory leak in js
        // however, if the function does not have any functions defined inside, this possibility is eliminated
        // and we can alloc local variables from the stack rather than the heap area
        // this variable simply says is the function is a "leaf", meaning that does not have any child functions
        int isLeafFunction;

        static FunctionAstNode *from(ZParser::FunctionContext *functionContext, string fileName);
    };
}