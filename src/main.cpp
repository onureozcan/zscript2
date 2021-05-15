
#include <string>

#include <vm/vm.h>
#include <compiler/compiler.h>

#include <ctime>

using namespace zero;
using namespace std;

int main(int argc, const char *argv[]) {
    Logger main_logger("main");

    if (argc < 2) {
        main_logger.error(" was expecting a file name as the first arg");
        return 1;
    }

    const char *filename = argv[1];
    auto program = Compiler().compileFile(string(filename));

    bool interpret_only = false;
    for (int i = 0; i < argc; i++) {
        if ("--interpret" == string(argv[i])) {
            main_logger.info("interpret only mode active");
            interpret_only = true;
            break;
        }
    }

    clock_t begin = clock();
#ifdef JIT_AVAILABLE
    if (interpret_only) {
        vm_interpret(program);
    } else {
        vm_run(program);
    }
#else
    vm_interpret(program);
#endif
    clock_t end = clock();

    double elapsedSecs = double(end - begin) / CLOCKS_PER_SEC;
    cout << string(filename) << " took " << elapsedSecs << "sec(s)\n";
    return 0;
}
