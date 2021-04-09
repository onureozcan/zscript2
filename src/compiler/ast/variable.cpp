#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    VariableAstNode *
    VariableAstNode::from(ZParser::VariableDeclarationContext *variableDeclarationContext, string fileName) {
        auto *variable = new VariableAstNode();

        variable->fileName = fileName;
        variable->line = variableDeclarationContext->getStart()->getLine();
        variable->pos = variableDeclarationContext->getStart()->getCharPositionInLine();

        variable->identifier = variableDeclarationContext->typedIdent()->ident->getText();
        if (variableDeclarationContext->typedIdent()->type != nullptr) {
            variable->typeName = variableDeclarationContext->typedIdent()->type->getText();
        }

        if (variableDeclarationContext->expression() != nullptr) {
            variable->initialValue = ExpressionAstNode::from(variableDeclarationContext->expression(), fileName);
        }
        return variable;
    }

    string VariableAstNode::toString() {
        return "var " + identifier + " = " + (initialValue != nullptr ? initialValue->toString() : "null");
    }
}