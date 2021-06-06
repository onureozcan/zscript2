#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    ClassDeclarationAstNode *ClassDeclarationAstNode::from(ZParser::ClassDeclarationContext *classContext,
                                                           string fileName) {
        auto classDeclaration = new ClassDeclarationAstNode();
        classDeclaration->name = classContext->name->getText();

        classDeclaration->fileName = fileName;
        classDeclaration->line = classContext->getStart()->getLine();
        classDeclaration->pos = classContext->getStart()->getCharPositionInLine();

        // return type ast
        auto returnTypeAst = TypeDescriptorAstNode::from(classDeclaration->name);

        // create allocation program
        auto allocationProgram = new ProgramAstNode();
        allocationProgram->fileName = fileName;
        allocationProgram->line = classDeclaration->line;
        allocationProgram->pos = classDeclaration->pos;

        // create allocation function
        auto allocationFunction = new FunctionAstNode();
        allocationFunction->name = classDeclaration->name;
        allocationFunction->fileName = fileName;
        allocationFunction->line = classDeclaration->line;
        allocationFunction->pos = classDeclaration->pos;
        allocationFunction->program = allocationProgram;

        auto typeArgumentsBlock = classContext->typeArgumentsBlock();
        if (typeArgumentsBlock != nullptr) {
            for (auto typeArg: typeArgumentsBlock->typedIdent()) {
                auto argName = typeArg->ident->getText();
                TypeDescriptorAstNode *typeAst;
                if (typeArg->typeDescriptor() != nullptr) {
                    typeAst = TypeDescriptorAstNode::from(typeArg->typeDescriptor(), fileName);
                } else {
                    typeAst = TypeDescriptorAstNode::from(TypeInfo::ANY.name);
                }
                allocationFunction->typeArguments.push_back({argName, typeAst});
                returnTypeAst->parameters.push_back(TypeDescriptorAstNode::from(argName));
            }
        }

        for (auto &fn: classContext->function()) {
            auto fnc = FunctionAstNode::from(fn, fileName);
            auto statement = new StatementAstNode();
            statement->type = StatementAstNode::TYPE_NAMED_FUNCTION;
            statement->namedFunction = fnc;
            allocationProgram->statements.push_back(statement);
        }

        for (auto &varContext: classContext->variableDeclaration()) {
            auto var = VariableAstNode::from(varContext, fileName);
            auto statement = new StatementAstNode();
            statement->type = StatementAstNode::TYPE_VARIABLE_DECLARATION;
            statement->variable = var;
            allocationProgram->statements.push_back(statement);
        }

        allocationFunction->returnType = returnTypeAst;//TypeDescriptorAstNode::from(TypeInfo::T_VOID.name);
        classDeclaration->allocationFunction = allocationFunction;
        return classDeclaration;
    }

    string ClassDeclarationAstNode::toString() {
        return "class " + allocationFunction->toString();
    }
}