#include <string>
#include "antlr4-runtime.h"
#include "ZLexer.h"
#include "ZParser.h"

#include <common/program.h>
#include <common/logger.h>
#include <compiler/compiler.h>
#include <compiler/ast.h>
#include <compiler/type_meta.h>

using namespace std;
using namespace antlr4;

namespace zero {

    //// --- IMPL

    class Compiler::Impl {
    public:
        Program *compileFile(const string &fileName) {
            log.debug("compile called for '%s'", fileName.c_str());
            log.debug("contents:\n %s", readFile(fileName).c_str());

            std::ifstream stream;
            stream.open(fileName);

            ANTLRInputStream input(stream);
            ZLexer lexer(&input);
            CommonTokenStream tokens(&lexer);
            ZParser parser(&tokens);

            ZParser::RootContext *root = parser.root();
            ProgramAstNode *programAst = ProgramAstNode::from(root->program(), fileName);
            doCompile(programAst);

            return nullptr;
        }

    private:
        Logger log = Logger("compiler");
        TypeMetadataRepository typeRepository;
        TypeMetadataExtractor metadataExtractor = TypeMetadataExtractor(&typeRepository);
        ByteCodeGenerator byteCodeGenerator = ByteCodeGenerator(&typeRepository);

        void doCompile(ProgramAstNode *programAst) {
            extractAndRegisterTypeMetadata(programAst);
            log.debug("ast :\n %s", programAst->toString().c_str());
            generateByteCode(programAst);
        }

        void extractAndRegisterTypeMetadata(ProgramAstNode *program) {
            metadataExtractor.extractAndRegister(program);
        }

        void generateByteCode(ProgramAstNode *pNode) {

        }

        static string readFile(const string &fileName) {
            ifstream t(fileName);
            stringstream buffer;
            buffer << t.rdbuf();
            return buffer.str();
        }
    };

    //// --- PUBLIC
    Compiler::Compiler() {
        impl = new Compiler::Impl();
    }

    Program *Compiler::compileFile(const string &fileName) {
        return impl->compileFile(fileName);
    }
}