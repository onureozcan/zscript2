#include <compiler/type_meta.h>

namespace zero {

    TypeInfo *ContextChain::current() {
        if (contextStack.empty()) return nullptr;
        return contextStack.back();
    }

    void ContextChain::push(ProgramAstNode *ast) {
        auto newContext = new TypeInfo("FunContext@" +
                                       ast->fileName + "(" + to_string(ast->line) + "&" +
                                       to_string(ast->pos) + ")", 0);

        typeMetadataRepository->registerType(newContext);
        ast->contextObjectTypeName = newContext->name;
        if (current() != nullptr)
            newContext->addProperty("$parent", current());
        contextStack.push_back(newContext);
    }

    void ContextChain::pop() {
        contextStack.pop_back();
    }

    int ContextChain::size() {
        return contextStack.size();
    }

    TypeInfo *ContextChain::at(int depth) {
        return contextStack[contextStack.size() - depth - 1];
    }
}