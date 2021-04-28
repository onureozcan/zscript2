#include <compiler/ast.h>
#include <common/logger.h>

#include <ZParser.h>

namespace zero {

    Logger programAstLogger("program_ast_logger");

    ProgramAstNode *ProgramAstNode::from(ZParser::ProgramContext *programContext, string fileName) {
        auto *program = new ProgramAstNode();

        program->fileName = fileName;
        program->line = programContext->getStart()->getLine();
        program->pos = programContext->getStart()->getCharPositionInLine();

        program->statements = new vector<StatementAstNode *>();

        int prevLine = -1;
        for (auto &piece : programContext->statement()) {
            StatementAstNode *statementNode = StatementAstNode::from(piece, fileName);
            if (statementNode != nullptr) {
                program->statements->push_back(statementNode);
                if (statementNode->line == prevLine) {
                    // TODO: this is grammar's responsibility, fix this
                    programAstLogger.error("found 2 statements at the same line, was not expecting this! at %s, %d,%d",
                                           fileName.c_str(), statementNode->line, statementNode->pos);
                    exit(1);
                }
                prevLine = statementNode->line;
            }
        }

        return program;
    }

    string ProgramAstNode::toString() {
        string statementsStr;
        for (auto &piece : *statements) statementsStr += piece->toString() + "\n";
        return statementsStr;
    }
}
