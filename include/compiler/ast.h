#pragma once

#include <vector>

#include "ZParser.h"

#include <compiler/type.h>
#include <common/logger.h>

using namespace std;

namespace zero {

    static Logger astLogger = Logger("ast_logger");

    class BaseAstNode {
    public:
        string fileName;
        int line;
        int pos;

        string errorInfoStr() const {
            return fileName + to_string(line) + "," + to_string(pos);
        }

        virtual string toString() {
            return string("ast node at " + fileName + to_string(line) + "," + to_string(pos));
        }
    };

    class TypeDescriptorAstNode : public BaseAstNode {
    public:
        string name;
        vector<TypeDescriptorAstNode *> parameters;

        static TypeDescriptorAstNode *from(ZParser::TypeDescriptorContext *typeDescriptorContext, string fileName);

        static TypeDescriptorAstNode *from(string typeName);

        string toString() override;
    };

    class ExpressionAstNode : public BaseAstNode {
    public:
        int expressionType;

        unsigned int memoryIndex; // memory index in the current context
        unsigned int memoryDepth; // how many contexts i need to go up to find this property - only relevant for direct access

        int isLvalue;

        TypeInfo *resolvedType;
        TypeInfo::PropertyDescriptor *propertyInfo = nullptr;

        static const int TYPE_ATOMIC = 0;
        static const int TYPE_UNARY = 1;
        static const int TYPE_BINARY = 2;
        static const int TYPE_FUNCTION_CALL = 3;

        static ExpressionAstNode *from(ZParser::ExpressionContext *expressionContext, string fileName);

        string toString() override;

        bool isOverloaded() const {
            return propertyInfo != nullptr && propertyInfo->allOverloads().size() > 1;
        }
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
        vector<TypeDescriptorAstNode *> typeParams;

        vector<TypeInfo *> resolvedTypeParams;

        TypeInfo *preferredCalleeOverload;
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
        static const int TYPE_NULL = 6;
    };

    class VariableAstNode : public BaseAstNode {
    public:
        string identifier;
        int hasExplicitTypeInfo;
        unsigned int memoryIndex;

        ExpressionAstNode *initialValue;
        TypeDescriptorAstNode *typeDescriptorAstNode;

        TypeInfo *resolvedType;

        static VariableAstNode *from(ZParser::VariableDeclarationContext *variableDeclarationContext, string fileName);

        string toString() override;
    };

    class IfStatementAstNode;

    class LoopAstNode;

    class FunctionAstNode;

    class ClassDeclarationAstNode;

    class StatementAstNode : public BaseAstNode {
    public:
        ExpressionAstNode *expression;
        VariableAstNode *variable;
        IfStatementAstNode *ifStatement;
        LoopAstNode *loop;
        FunctionAstNode *namedFunction;
        ClassDeclarationAstNode *classDeclaration;
        int type;

        static const int TYPE_EXPRESSION = 0;
        static const int TYPE_VARIABLE_DECLARATION = 1;
        static const int TYPE_RETURN = 2;
        static const int TYPE_IF = 3;
        static const int TYPE_LOOP = 4;
        static const int TYPE_BREAK = 5;
        static const int TYPE_CONTINUE = 6;
        static const int TYPE_NAMED_FUNCTION = 7;
        static const int TYPE_CLASS_DECLARATION = 8;

        static StatementAstNode *from(ZParser::StatementContext *statementContext, string fileName);

        string toString() override;
    };

    class ProgramAstNode : public BaseAstNode {
    public:
        vector<StatementAstNode *> statements;

        string contextObjectTypeName;

        static ProgramAstNode *from(ZParser::ProgramContext *programContext, string fileName);

        string toString() override;
    };

    class IfStatementAstNode : public BaseAstNode {
    public:
        ExpressionAstNode *expression;
        ProgramAstNode *program, *elseProgram;

        static IfStatementAstNode *from(ZParser::IfStatementContext *ifContext, string fileName);

        string toString() override;
    };

    class LoopAstNode : public BaseAstNode {
    public:
        VariableAstNode *loopVariable;
        ExpressionAstNode *loopIterationExpression, *loopConditionExpression;
        ProgramAstNode *program;

        static LoopAstNode *from(ZParser::ForLoopContext *forLoopContext, string fileName);

        string toString() override;
    };

    class FunctionAstNode : public AtomicExpressionAstNode {
    public:
        ProgramAstNode *program;
        vector<pair<string, TypeDescriptorAstNode *>> arguments;
        vector<pair<string, TypeDescriptorAstNode *>> typeArguments;
        TypeDescriptorAstNode *returnType;

        // this one is important, let me explain:
        // if a function has a function definition inside, it is perfectly legal for the child function to access variables in the "upper" scope
        // in that case, we cannot simply destroy the parent function context. it is a well-known pattern of memory leak in js
        // however, if the function does not have any functions defined inside, this possibility is eliminated
        // and we can alloc local variables from the stack rather than the heap area
        // this variable simply says is the function is a "leaf", meaning that does not have any child functions
        int isLeafFunction;
        string name;

        int hasExplicitReturnType;

        static FunctionAstNode *from(ZParser::FunctionContext *functionContext, string fileName);
    };

    class ClassDeclarationAstNode : public BaseAstNode {
    public:
        /*vector<FunctionAstNode *> namedFunctions;
        vector<VariableAstNode *> variableDeclarations;
        vector<pair<string, TypeDescriptorAstNode *>> typeArguments;
        TypeInfo *resolvedType;*/

        // this will be invoked to allocate memory and initialize fields
        FunctionAstNode* allocationFunction;

        string name;

        static ClassDeclarationAstNode *from(ZParser::ClassDeclarationContext *classContext, string fileName);

        string toString() override;
    };
}