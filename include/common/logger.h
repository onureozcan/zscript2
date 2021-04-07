#pragma once

#include <string>
#include <vector>

using namespace std;

namespace zero {
    class LoggerImpl;

    class Logger {
    public:

        static int LOG_LEVEL_DEBUG;
        static int LOG_LEVEL_INFO;
        static int LOG_LEVEL_WARN;
        static int LOG_LEVEL_ERROR;

        Logger(string name);
        Logger(string name, int level);

        void info(const char *format, ...);
        void debug(const char *format, ...);
        void warn(const char *format, ...);
        void error(const char *format, ...);

    private:
        LoggerImpl* impl;
    };
}