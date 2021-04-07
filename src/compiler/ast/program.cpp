#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    ProgramAstNode *ProgramAstNode::from(ZParser::ProgramContext *programContext, string fileName) {
        auto *program = new ProgramAstNode();

        program->fileName = fileName;
        program->line = programContext->getStart()->getLine();
        program->pos = programContext->getStart()->getCharPositionInLine();

        program->statements = new vector<StatementAstNode *>();

        for (auto &piece : programContext->statement()) {
            StatementAstNode *statementNode = StatementAstNode::from(piece, fileName);
            if (statementNode != nullptr)
                program->statements->push_back(statementNode);
        }

        return program;
    }

    string ProgramAstNode::toString() {
        string statementsStr;
        for (auto &piece : *statements) statementsStr += piece->toString() + "\n";
        return statementsStr;
    }
}
