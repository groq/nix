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

/**
 * A json logger intended for external consumption
 * (contrary to 'JSONLogger' which is an internal thing
 */
struct ExternalJSONLogger : JSONLogger {
    ExternalJSONLogger(Logger &prevLogger) : JSONLogger(prevLogger) {}

    nlohmann::json jsonActivityType(ActivityType type) override {
        switch (type) {
        case actUnknown:
            return "actUnknown";
        case actCopyPath:
            return "actCopyPath";
        case actDownload:
            return "actDownload";
        case actRealise:
            return "actRealise";
        case actCopyPaths:
            return "actCopyPaths";
        case actBuilds:
            return "actBuilds";
        case actBuild:
            return "actBuild";
        case actOptimiseStore:
            return "actOptimiseStore";
        case actVerifyPaths:
            return "actVerifyPaths";
        case actSubstitute:
            return "actSubstitute";
        case actQueryPathInfo:
            return "actQueryPathInfo";
        case actPostBuildHook:
            return "actPostBuildHook";
        default:
            return "UnknownActivity";
        }
    }

    nlohmann::json jsonResultType(ResultType type) override {
        switch (type) {
        case resFileLinked:
            return "resFileLinked";
        case resBuildLogLine:
            return "resBuildLogLine";
        case resUntrustedPath:
            return "resUntrustedPath";
        case resCorruptedPath:
            return "resCorruptedPath";
        case resSetPhase:
            return "resSetPhase";
        case resProgress:
            return "resProgress";
        case resSetExpected:
            return "resSetExpected";
        case resPostBuildLogLine:
            return "resPostBuildLogLine";
        default:
            return "UnknownResultType";
        }
    }

    void result(ActivityId act, ResultType type,
                const Fields &fields) override{};
};

Logger *makeDefaultLogger() {
    switch (logFormat) {
    case logFormatRaw:
        return makeSimpleLogger();
    case logFormatJSON:
        return new ExternalJSONLogger(*makeSimpleLogger());
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
