#include "loggers.hh"
#include "json-logger.hh"
#include "logging.hh"
#include "progress-bar.hh"
#include "util.hh"

#include <atomic>
#include <nlohmann/json.hpp>

namespace nix {

typedef enum {
    logFormatRaw,
    logFormatJSON,
    logFormatBar,
    logFormatBarWithLogs,
} LogFormat;

LogFormat logFormat = logFormatRaw;

LogFormat parseLogFormat(const string &logFormatStr) {
    if (logFormatStr == "raw")
        return logFormatRaw;
    else if (logFormatStr == "json")
        return logFormatJSON;
    else if (logFormatStr == "bar")
        return logFormatBar;
    else if (logFormatStr == "bar-with-logs")
        return logFormatBarWithLogs;
    throw Error(format("option 'log-format' has an invalid value '%s'") %
                logFormatStr);
}

Logger *makeDefaultLogger() {
    switch (logFormat) {
    case logFormatRaw:
        return makeSimpleLogger();
    case logFormatJSON:
        return new JSONLogger();
    case logFormatBar:
        return createProgressBar();
    case logFormatBarWithLogs:
        return createProgressBar(true);
    default:
        throw Error(format("Invalid log format '%i'") % logFormat);
    }
}

void setLogFormat(const string &logFormatStr) {
    logFormat = parseLogFormat(logFormatStr);
    logger = makeDefaultLogger();
}

} // namespace nix
