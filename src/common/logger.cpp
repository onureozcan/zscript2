#include <common/logger.h>
#include <cstdarg>
#include <ctime>

namespace zero {

    vector<string> LOG_LEVEL_MAP = {"DEBUG", "INFO", "WARN", "ERROR"};

    class LoggerImpl {
    public:
        string name;
        int level;

        void doLog(int priority, const char *format, va_list args) const {
            if (priority >= level) {
                time_t now = time(nullptr);
                tm *localTime = localtime(&now);
                string timeStr = string(asctime(localTime));
                timeStr.pop_back();
                fprintf(stderr, "%s, %s, [%s]: ", timeStr.c_str(), name.c_str(), LOG_LEVEL_MAP[priority].c_str());
                vfprintf(stderr, format, args);
                va_end(args);
                fprintf(stderr, "\n");
            }
        }
    };

    //// ---- PUBLIC

    int Logger::LOG_LEVEL_INFO = 1;
    int Logger::LOG_LEVEL_DEBUG = 0;
    int Logger::LOG_LEVEL_ERROR = 3;
    int Logger::LOG_LEVEL_WARN = 2;

    Logger::Logger(string name) : Logger(name, LOG_LEVEL_DEBUG) {}

    Logger::Logger(string name, int level) {
        impl = new LoggerImpl();
        impl->name = name;
        impl->level = level;
    }

    void Logger::info(const char *format, ...) {
        va_list args;
        va_start(args, format);
        impl->doLog(LOG_LEVEL_INFO, format, args);
    }

    void Logger::error(const char *format, ...) {
        va_list args;
        va_start(args, format);
        impl->doLog(LOG_LEVEL_ERROR, format, args);
    }

    void Logger::debug(const char *format, ...) {
        va_list args;
        va_start(args, format);
        impl->doLog(LOG_LEVEL_DEBUG, format, args);
    }

    void Logger::warn(const char *format, ...) {
        va_list args;
        va_start(args, format);
        impl->doLog(LOG_LEVEL_WARN, format, args);
    }
}