#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    IfStatementAstNode *IfStatementAstNode::from(ZParser::IfStatementContext *ifContext, string fileName) {
        auto ifStmt = new IfStatementAstNode();
        ifStmt->fileName = fileName;
        ifStmt->pos = ifContext->getStart()->getCharPositionInLine();
        ifStmt->line = ifContext->getStart()->getLine();
        ifStmt->program = ProgramAstNode::from(ifContext->program(0), fileName);
        ifStmt->expression = ExpressionAstNode::from(ifContext->expression(), fileName);

        if (ifContext->ifStatement() != nullptr) {
            // else if
            ProgramAstNode* elseProgram = new ProgramAstNode();
            elseProgram->fileName = fileName;
            elseProgram->line = ifContext->ifStatement()->getStart()->getLine();
            elseProgram->pos = ifContext->ifStatement()->getStart()->getCharPositionInLine();
            elseProgram->statements = new vector<StatementAstNode *>();
            auto elseStatement = new StatementAstNode();
            elseStatement->line = elseProgram->line;
            elseStatement->pos = elseProgram->pos;
            elseStatement->fileName = elseProgram->fileName;
            elseStatement->type = StatementAstNode::TYPE_IF;
            elseStatement->ifStatement = from(ifContext->ifStatement(), fileName);
            elseProgram->statements->push_back(elseStatement);
            ifStmt->elseProgram = elseProgram;
        } else if (ifContext->program().size() > 1) {
            // else
            ProgramAstNode* elseProgram = ProgramAstNode::from(ifContext->program(1), fileName);
            ifStmt->elseProgram = elseProgram;
        }

        return ifStmt;
    }

    string IfStatementAstNode::toString() {
        auto ifStr = "if (" + expression->toString() + ") {\n"
                     + program->toString()
                     + "}";
        if (elseProgram != nullptr) {
            ifStr += " else {\n" + elseProgram->toString() + "}";
        }
        return ifStr;
    }
}
