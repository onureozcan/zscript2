
#include <string>

#include <vm/vm.h>
#include <compiler/compiler.h>
#include <common/logger.h>

using namespace zero;
using namespace std;

Logger logger("main");

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        logger.error(" was expecting a file name as the first arg");
        return 1;
    }
    const char *filename = argv[1];
    auto program = Compiler().compileFile(string(filename));
    vm_run(program);
    return 0;
}
