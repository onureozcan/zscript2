#pragma once

#include <common/program.h>
#include <string>

using namespace std;

namespace zero {

    class CompilerImpl;

    class Compiler {
    public:
        Compiler();

        Program *compileFile(const string& fileName);

    private:
        CompilerImpl* impl;
    };
}