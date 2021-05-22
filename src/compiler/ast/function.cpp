#include <compiler/ast.h>
#include <compiler/type.h>

#include <ZParser.h>

namespace zero {

    FunctionAstNode *FunctionAstNode::from(ZParser::FunctionContext *functionContext, string fileName) {
        auto *function = new FunctionAstNode();

        function->fileName = fileName;
        function->line = functionContext->getStart()->getLine();
        function->pos = functionContext->getStart()->getCharPositionInLine();

        function->expressionType = TYPE_ATOMIC;
        function->atomicType = TYPE_FUNCTION;

        function->arguments = new vector<pair<string, TypeDescriptorAstNode *>>();
        for (auto &piece: functionContext->typedIdent()) {
            string argName = piece->ident->getText();
            auto typeAst = piece->type == nullptr ?
                           TypeDescriptorAstNode::from(TypeInfo::ANY.name) : TypeDescriptorAstNode::from(piece->type,
                                                                                                         fileName);
            function->arguments->push_back({argName, typeAst});
        }
        if (functionContext->type != nullptr) {
            function->returnType = TypeDescriptorAstNode::from(functionContext->type, fileName);
        } else {
            function->returnType = TypeDescriptorAstNode::from(TypeInfo::T_VOID.name);
        }

        auto typeParameterBlock = functionContext->typeParameterBlock();
        if (typeParameterBlock != nullptr) {
            for (auto typeParam: typeParameterBlock->typedIdent()) {
                auto parameterName = typeParam->ident->getText();
                TypeDescriptorAstNode *typeAst;
                if (typeParam->typeDescriptor() != nullptr) {
                    typeAst = TypeDescriptorAstNode::from(typeParam->typeDescriptor(), fileName);
                } else {
                    typeAst = TypeDescriptorAstNode::from(TypeInfo::ANY.name);
                }
                function->typeParameters.push_back({parameterName, typeAst});
            }
        }

        function->program = ProgramAstNode::from(functionContext->program(), fileName);

        return function;
    }
}