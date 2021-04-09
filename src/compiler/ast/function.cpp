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

        function->arguments = new vector<pair<string, string>>();
        for (auto &piece: functionContext->typedIdent()) {
            string argName = piece->ident->getText();
            string typeName = piece->type == nullptr ? "any" : piece->type->getText();
            function->arguments->push_back({argName, typeName});
        }
        function->program = ProgramAstNode::from(functionContext->program(), fileName);

        return function;
    }
}