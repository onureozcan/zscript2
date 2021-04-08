#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    int ExpressionAstNode::TYPE_ATOMIC = 0;
    int ExpressionAstNode::TYPE_UNARY = 1;
    int ExpressionAstNode::TYPE_BINARY = 2;
    int ExpressionAstNode::TYPE_FUNCTION_CALL = -1;

    int AtomicExpressionAstNode::TYPE_STRING = 0;
    int AtomicExpressionAstNode::TYPE_IDENTIFIER = 1;
    int AtomicExpressionAstNode::TYPE_FUNCTION = 2;
    int AtomicExpressionAstNode::TYPE_DECIMAL = 0;
    int AtomicExpressionAstNode::TYPE_INT = 0;


    AtomicExpressionAstNode *createAtom(ZParser::AtomContext *atomContext, string fileName) {
        if (atomContext->function() != nullptr) {
            return FunctionAstNode::from(atomContext->function(), fileName);
        }

        auto atomic = new AtomicExpressionAstNode();
        atomic->expressionType = ExpressionAstNode::TYPE_ATOMIC;
        atomic->data = atomContext->getText();

        atomic->fileName = fileName;
        atomic->line = atomContext->getStart()->getLine();
        atomic->pos = atomContext->getStart()->getCharPositionInLine();

        if (atomContext->IDENT() != nullptr) {
            atomic->atomicType = AtomicExpressionAstNode::TYPE_IDENTIFIER;
        }
        if (atomContext->STRING() != nullptr) {
            atomic->atomicType = AtomicExpressionAstNode::TYPE_STRING;
            atomic->data = atomContext->STRING()->getText();
        }
        if (atomContext->INT() != nullptr) {
            atomic->atomicType = AtomicExpressionAstNode::TYPE_INT;
        }
        if (atomContext->DECIMAL() != nullptr) {
            atomic->atomicType = AtomicExpressionAstNode::TYPE_DECIMAL;
        }
        return atomic;
    }

    ExpressionAstNode *ExpressionAstNode::from(ZParser::ExpressionContext *expressionContext, string fileName) {
        ExpressionAstNode *expression;

        if (expressionContext->primaryExpresssion() != nullptr) {
            if (expressionContext->primaryExpresssion()->expression() != nullptr) {
                return from(expressionContext->primaryExpresssion()->expression(), fileName);
            } else {
                return createAtom(expressionContext->primaryExpresssion()->atom(), fileName);
            }
        }

        if (expressionContext->bop != nullptr) {
            auto binary = new BinaryExpressionAstNode();
            binary->expressionType = TYPE_BINARY;
            binary->left = from(expressionContext->expression(0), fileName);
            binary->right = from(expressionContext->expression(1), fileName);
            binary->op = Operator::getBy(expressionContext->bop->getText(), 2);
            expression = binary;
        } else if (expressionContext->prefix != nullptr) {
            auto unary = new PrefixExpressionAstNode();
            unary->expressionType = TYPE_UNARY;
            unary->right = from(expressionContext->expression(0), fileName);
            unary->op = Operator::getBy(expressionContext->prefix->getText(), 2);
            expression = unary;
        } else {
            auto functionCall = new FunctionCallExpressionAstNode();
            functionCall->expressionType = TYPE_FUNCTION_CALL;
            functionCall->left = from(expressionContext->expression(0), fileName);
            functionCall->params = new vector<ExpressionAstNode *>();
            for (unsigned int i = 1; i < expressionContext->expression().size(); i++) {
                functionCall->params->push_back(from(expressionContext->expression(i), fileName));
            }
            expression = functionCall;
        }

        expression->fileName = fileName;
        expression->line = expressionContext->getStart()->getLine();
        expression->pos = expressionContext->getStart()->getCharPositionInLine();

        return expression;
    }

    string typeInfoStr(TypeInfo *typeInfo) {
        return ":" + ((typeInfo != nullptr) ? typeInfo->name : "?");
    }

    string ExpressionAstNode::toString() {
        if (expressionType == TYPE_FUNCTION_CALL) {
            auto call = ((FunctionCallExpressionAstNode *) this);
            string paramsStr;
            for (auto &piece: *call->params) {
                paramsStr += piece->toString() + ",";
            }
            return "(" + call->left->toString() + "(" + paramsStr + ")" + typeInfoStr(this->typeInfo) + ")";
        } else if (expressionType == TYPE_UNARY) {
            auto prefix = ((PrefixExpressionAstNode *) this);
            return "(" + prefix->op->name + "" + prefix->right->toString() + typeInfoStr(this->typeInfo) + ")";
        } else if (expressionType == TYPE_BINARY) {
            auto binary = ((BinaryExpressionAstNode *) this);
            return "(" + binary->left->toString() + "" + binary->op->name + "" +
                   binary->right->toString() + typeInfoStr(this->typeInfo) + ")";
        } else if (expressionType == TYPE_ATOMIC) {
            auto atom = ((AtomicExpressionAstNode *) this);
            if (atom->atomicType == AtomicExpressionAstNode::TYPE_FUNCTION) {
                auto function = ((FunctionAstNode *) atom);
                string argsStr;
                for (const auto &piece : *function->arguments) argsStr += piece;
                return "(fun" + function->identifier + "(" + argsStr + "){\n" + function->program->toString() + "}" +
                       typeInfoStr(this->typeInfo) + ")";
            } else {
                return "(" + atom->data + typeInfoStr(this->typeInfo) + ")";
            }
        } else {
            return BaseAstNode::toString();
        }
    }
}