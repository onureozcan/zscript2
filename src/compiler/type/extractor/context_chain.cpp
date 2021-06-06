#include <compiler/type_meta.h>

namespace zero {

    TypeInfo *ContextChain::current() {
        if (contextStack.empty()) return nullptr;
        return contextStack.back();
    }

    void ContextChain::push(ProgramAstNode *ast, string name) {
        if (name.empty()) {
            name = "FunContext@" +
                   ast->fileName + "(" + to_string(ast->line) + "&" +
                   to_string(ast->pos) + ")";
        }
        auto newContext = new TypeInfo(name, 0);
        typeInfoRepository->registerType(newContext);
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