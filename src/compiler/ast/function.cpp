#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    FunctionAstNode *FunctionAstNode::from(ZParser::FunctionContext *functionContext, string fileName) {
        auto *function = new FunctionAstNode();

        function->fileName = fileName;
        function->line = functionContext->getStart()->getLine();
        function->pos = functionContext->getStart()->getCharPositionInLine();

        function->expressionType = TYPE_ATOMIC;
        function->atomicType = TYPE_FUNCTION;

        function->arguments = new vector<string>();
        function->program = ProgramAstNode::from(functionContext->program(), fileName);

        return function;
    }

    string FunctionAstNode::toString() {
        string argsStr;
        for (const auto &piece : *arguments) argsStr += piece;
        return "\nfun" + this->identifier + "(" + argsStr + "){\n" + this->program->toString() + "\n}\n";
    }
}