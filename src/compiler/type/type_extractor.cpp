#include <compiler/type.h>

namespace zero {

    class TypeMetadataExtractor::Impl {
    public:
        void extractAndRegister(BaseAstNode *pNode) {

        }

    private:
    };

    void TypeMetadataExtractor::extractAndRegister(BaseAstNode *ast) {
        this->impl->extractAndRegister(ast);
    }

    TypeMetadataExtractor::TypeMetadataExtractor() {
        this->impl = new Impl();
    }
}