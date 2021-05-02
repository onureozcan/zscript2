#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    LoopAstNode *LoopAstNode::from(ZParser::ForLoopContext *forLoopContext, string fileName) {
        auto forLoop = new LoopAstNode();
        forLoop->fileName = fileName;
        forLoop->pos = forLoopContext->getStart()->getCharPositionInLine();
        forLoop->line = forLoopContext->getStart()->getLine();
        forLoop->program = ProgramAstNode::from(forLoopContext->program(), fileName);

        if (forLoopContext->variableDeclaration() != nullptr) {
            forLoop->loopVariable = VariableAstNode::from(forLoopContext->variableDeclaration(), fileName);
        }

        if (!forLoopContext->expression().empty()) {
            forLoop->loopConditionExpression = ExpressionAstNode::from(forLoopContext->expression(0), fileName);
        }

        if (forLoopContext->expression().size() > 1) {
            forLoop->loopIterationExpression = ExpressionAstNode::from(forLoopContext->expression(1), fileName);
        }

        return forLoop;
    }

    string LoopAstNode::toString() {
        return "for (" +
               (loopVariable != nullptr ? loopVariable->toString() : "") + ";" +
               (loopConditionExpression != nullptr ? loopConditionExpression->toString() : "") + ";" +
               (loopIterationExpression != nullptr ? loopIterationExpression->toString() : "") + ") {\n" +
               program->toString()
               + "}";
    }
}