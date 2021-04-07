/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

//
//  main.cpp
//  antlr4-cpp-demo
//
//  Created by Mike Lischke on 13.03.16.
//

#include <iostream>
#include <fstream>
#include <string>


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
    Compiler().compileFile(string(filename));
    return 0;
}
