#pragma once

#include <common/program.h>
#include <string>
#include "ast.h"
#include "type_meta.h"

using namespace std;

namespace zero {

    class Compiler {
    public:
        class Impl;

        Compiler();

        Program *compileFile(const string& fileName);

    private:
        Impl* impl;
    };

    class ByteCodeGenerator {
    public:
        class Impl;

        Program * generate(ProgramAstNode* programAstNode);

        ByteCodeGenerator();

    private:
        Impl* impl;
    };
}