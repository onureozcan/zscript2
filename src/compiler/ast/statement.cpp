#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    int StatementAstNode::TYPE_EXPRESSION = 0;
    int StatementAstNode::TYPE_VARIABLE_DECLARATION = 1;

    StatementAstNode *StatementAstNode::from(ZParser::StatementContext *statementContext, string fileName) {
        auto *statement = new StatementAstNode();

        statement->fileName = fileName;
        statement->line = statementContext->getStart()->getLine();
        statement->pos = statementContext->getStart()->getCharPositionInLine();

        if (statementContext->variableDeclaration() != nullptr) {
            statement->type = TYPE_VARIABLE_DECLARATION;
            statement->variable = VariableAstNode::from(statementContext->variableDeclaration(), fileName);
        } else if (statementContext->expression() != nullptr) {
            statement->type = TYPE_EXPRESSION;
            statement->expression = ExpressionAstNode::from(statementContext->expression(), fileName);
        } else {
            free(statement);
            return nullptr;
        }
        return statement;
    }

    string StatementAstNode::toString() {
        if (type == TYPE_EXPRESSION) {
            return expression->toString();
        } else {
            return variable->toString();
        }
    }
}