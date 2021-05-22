#include <compiler/ast.h>
#include <ZParser.h>

namespace zero {

    TypeDescriptorAstNode *
    TypeDescriptorAstNode::from(ZParser::TypeDescriptorContext *typeDescriptorContext, string fileName) {
        auto typeNode = new TypeDescriptorAstNode();
        typeNode->fileName = fileName;
        typeNode->line = typeDescriptorContext->start->getLine();
        typeNode->pos = typeDescriptorContext->start->getCharPositionInLine();
        typeNode->name = typeDescriptorContext->typeName->getText();
        int paramCount = typeDescriptorContext->typeDescriptor().size();
        for (int i = 0; i < paramCount; i++) {
            typeNode->parameters.push_back(
                    TypeDescriptorAstNode::from(typeDescriptorContext->typeDescriptor(i), fileName));
        }
        return typeNode;
    }

    string TypeDescriptorAstNode::toString() {
        if (parameters.empty())
            return this->name;
        string parametersStr = "<";
        for (auto &param: parameters) {
            parametersStr += param->toString() + ",";
        }
        parametersStr += ">";
        return this->name + parametersStr;
    }

    TypeDescriptorAstNode *TypeDescriptorAstNode::from(string typeName) {
        auto typeNode = new TypeDescriptorAstNode();
        typeNode->name = typeName;
        return typeNode;
    }
}