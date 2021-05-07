
#include <string>

#include <vm/vm.h>
#include <compiler/compiler.h>
#include <common/logger.h>

#include <ctime>

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

    clock_t begin = clock();
    vm_run(program);
    clock_t end = clock();

    double elapsedSecs = double(end - begin) / CLOCKS_PER_SEC;
    cout << string(filename) << " took " << elapsedSecs << "sec(s)\n";
    return 0;
}
