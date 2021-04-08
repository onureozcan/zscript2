#include <common/program.h>
#include <common/logger.h>
#include <compiler/compiler.h>
#include <string>
#include "antlr4-runtime.h"
#include "ZLexer.h"
#include "ZParser.h"

#include <compiler/ast.h>

using namespace std;
using namespace antlr4;

namespace zero {

    //// --- IMPL

    class CompileError : public exception {
    public:
        CompileError(const char *const message) : exception(message) {}
    };

    class Context {
    private:
        map<string, TypeInfo> variableTypeInfoMap;
    public:
        void addVariable(string identifier, TypeInfo typeInfo) {
            auto isDefinedBefore = variableTypeInfoMap.find(identifier) != variableTypeInfoMap.end();
            if (!isDefinedBefore) {
                variableTypeInfoMap[identifier] = typeInfo;
            } else {
                throw CompileError(("variable " + identifier + "is already defined!").c_str());
            }
        }
    };

    class CompilerImpl {
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
        TypeMetadataExtractor metadataExtractor = TypeMetadataExtractor();

        void doCompile(ProgramAstNode *programAst) {
            extractAndRegisterTypeMetadata(programAst);
            log.debug("ast :\n %s", programAst->toString().c_str());
            generateByteCode(programAst);
        }

        void extractAndRegisterTypeMetadata(ProgramAstNode *pNode) {
            // TODO: iterate over all the functions and extract type info
            // this will be helpful when we import some function from other files
            // it also allow us to use a class that is later defined in the file
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
        impl = new CompilerImpl();
    }

    Program *Compiler::compileFile(const string &fileName) {
        return impl->compileFile(fileName);
    }
}