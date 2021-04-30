#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    StatementAstNode *StatementAstNode::from(ZParser::StatementContext *statementContext, string fileName) {
        auto *statement = new StatementAstNode();

        statement->fileName = fileName;
        statement->line = statementContext->getStart()->getLine();
        statement->pos = statementContext->getStart()->getCharPositionInLine();

        if (statementContext->variableDeclaration() != nullptr) {
            statement->type = TYPE_VARIABLE_DECLARATION;
            statement->variable = VariableAstNode::from(statementContext->variableDeclaration(), fileName);
        } else if (statementContext->ret != nullptr) {
            statement->type = TYPE_RETURN;
            if (statementContext->expression() != nullptr) {
                statement->expression = ExpressionAstNode::from(statementContext->expression(), fileName);
            }
        } else if (statementContext->expression() != nullptr) {
            statement->type = TYPE_EXPRESSION;
            statement->expression = ExpressionAstNode::from(statementContext->expression(), fileName);
        } else if (statementContext->ifStatement() != nullptr) {
            statement->type = TYPE_IF;
            statement->ifStatement = IfStatementAstNode::from(statementContext->ifStatement(), fileName);
        } else {
            free(statement);
            return nullptr;
        }
        return statement;
    }

    string StatementAstNode::toString() {
        if (type == TYPE_EXPRESSION) {
            return expression->toString();
        } else if (type == TYPE_RETURN) {
            return "return " + (expression == nullptr ? "" : expression->toString());
        } else if (type == TYPE_IF) {
            return ifStatement->toString();
        } else {
            return variable->toString();
        }
    }
}