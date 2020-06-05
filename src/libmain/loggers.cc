#include "loggers.hh"
#include "../nix/progress-bar.hh"

namespace nix {

LogFormat defaultLogFormat = LogFormat::raw;

LogFormat parseLogFormat(const string &logFormatStr) {
    if (logFormatStr == "raw")
        return LogFormat::raw;
    else if (logFormatStr == "internal-json")
        return LogFormat::internalJson;
    else if (logFormatStr == "bar")
        return LogFormat::bar;
    else if (logFormatStr == "bar-with-logs")
        return LogFormat::barWithLogs;
    throw Error(format("option 'log-format' has an invalid value '%s'") %
                logFormatStr);
}

Logger *makeDefaultLogger() {
    switch (defaultLogFormat) {
    case LogFormat::raw:
        return makeSimpleLogger();
    case LogFormat::internalJson:
        return makeJSONLogger(*makeSimpleLogger());
    case LogFormat::bar:
        return makeProgressBar();
    case LogFormat::barWithLogs:
        return makeProgressBar(true);
    default:
        abort();
    }
}

void setLogFormat(const string &logFormatStr) {
    setLogFormat(parseLogFormat(logFormatStr));
}

void setLogFormat(const LogFormat &logFormat) {
    defaultLogFormat = logFormat;
    createDefaultLogger();
}

void createDefaultLogger() {
    logger = makeDefaultLogger();
}

}
