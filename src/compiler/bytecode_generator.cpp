#include <common/program.h>
#include <compiler/compiler.h>
#include <common/logger.h>

using namespace std;

namespace zero {

    class ByteCodeGenerator::Impl {

    public:
        TypeMetadataRepository *typeRepository;

        Program *generate(ProgramAstNode *programAstNode) {
            return nullptr;
        }

    private:
        Logger log = Logger("bytecode_generator");
    };

    Program *ByteCodeGenerator::generate(ProgramAstNode *programAstNode) {
        return impl->generate(programAstNode);
    }

    ByteCodeGenerator::ByteCodeGenerator(TypeMetadataRepository *typeMetadataRepository) {
        this->impl = new ByteCodeGenerator::Impl();
        this->impl->typeRepository = typeMetadataRepository;
    }
}