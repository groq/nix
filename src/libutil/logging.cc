#include "logging.hh"
#include "util.hh"
#include "progress-bar.hh"

#include <atomic>
#include <nlohmann/json.hpp>

namespace nix {

static thread_local ActivityId curActivity = 0;

ActivityId getCurActivity()
{
    return curActivity;
}
void setCurActivity(const ActivityId activityId)
{
    curActivity = activityId;
}

Logger * logger = makeDefaultLogger();

void Logger::warn(const std::string & msg)
{
    log(lvlWarn, ANSI_RED "warning:" ANSI_NORMAL " " + msg);
}

class SimpleLogger : public Logger
{
public:

    bool systemd, tty;

    SimpleLogger()
    {
        systemd = getEnv("IN_SYSTEMD") == "1";
        tty = isatty(STDERR_FILENO);
    }

    void log(Verbosity lvl, const FormatOrString & fs) override
    {
        if (lvl > verbosity) return;

        std::string prefix;

        if (systemd) {
            char c;
            switch (lvl) {
            case lvlError: c = '3'; break;
            case lvlWarn: c = '4'; break;
            case lvlInfo: c = '5'; break;
            case lvlTalkative: case lvlChatty: c = '6'; break;
            default: c = '7';
            }
            prefix = std::string("<") + c + ">";
        }

        writeToStderr(prefix + filterANSIEscapes(fs.s, !tty) + "\n");
    }

    void startActivity(ActivityId act, Verbosity lvl, ActivityType type,
        const std::string & s, const Fields & fields, ActivityId parent)
        override
    {
        if (lvl <= verbosity && !s.empty())
            log(lvl, s + "...");
    }
};

Verbosity verbosity = lvlInfo;

typedef enum {
    logFormatRaw,
    logFormatJSON,
    logFormatBar,
    logFormatBarWithLogs,
} LogFormat;

LogFormat logFormat = logFormatRaw;

LogFormat parseLogFormat(const string &logFormatStr)
{
    if (logFormatStr == "raw")
      return logFormatRaw;
    else if (logFormatStr == "json")
      return logFormatJSON;
    else if (logFormatStr == "bar")
      return logFormatBar;
    else if (logFormatStr == "bar-with-logs")
      return logFormatBarWithLogs;
    throw Error(format("option 'log-format' has an invalid value '%s'") % logFormatStr);
}

void setLogFormat(const string &logFormatStr)
{
    logFormat = parseLogFormat(logFormatStr);
    logger = makeDefaultLogger();
}

void warnOnce(bool & haveWarned, const FormatOrString & fs)
{
    if (!haveWarned) {
        warn(fs.s);
        haveWarned = true;
    }
}

void writeToStderr(const string & s)
{
    try {
        writeFull(STDERR_FILENO, s, false);
    } catch (SysError & e) {
        /* Ignore failing writes to stderr.  We need to ignore write
           errors to ensure that cleanup code that logs to stderr runs
           to completion if the other side of stderr has been closed
           unexpectedly. */
    }
}

Logger * makeDefaultLogger()
{
    switch (logFormat) {
        case logFormatRaw:
            return new SimpleLogger();
        case logFormatJSON:
            return makeExternalJSONLogger(*(new SimpleLogger()));
        case logFormatBar:
            return createProgressBar();
        case logFormatBarWithLogs:
            return createProgressBar(true);
        default:
            throw Error(format("Invalid log format '%i'") % logFormat);
    }
}

std::atomic<uint64_t> nextId{(uint64_t) getpid() << 32};

Activity::Activity(Logger & logger, Verbosity lvl, ActivityType type,
    const std::string & s, const Logger::Fields & fields, ActivityId parent)
    : logger(logger), id(nextId++)
{
    logger.startActivity(id, lvl, type, s, fields, parent);
}

struct JSONLogger : Logger
{
    Logger & prevLogger;

    JSONLogger(Logger & prevLogger) : prevLogger(prevLogger) { }

    virtual nlohmann::json jsonActivityType(ActivityType type) {
        return type;
    }

    virtual nlohmann::json jsonResultType(ResultType type) {
        return type;
    }

    void addFields(nlohmann::json & json, const Fields & fields)
    {
        if (fields.empty()) return;
        auto & arr = json["fields"] = nlohmann::json::array();
        for (auto & f : fields)
            if (f.type == Logger::Field::tInt)
                arr.push_back(f.i);
            else if (f.type == Logger::Field::tString)
                arr.push_back(f.s);
            else
                abort();
    }

    void write(const nlohmann::json & json)
    {
        prevLogger.log(lvlError, "@nix " + json.dump());
    }

    void log(Verbosity lvl, const FormatOrString & fs) override
    {
        nlohmann::json json;
        json["action"] = "msg";
        json["level"] = lvl;
        json["msg"] = fs.s;
        write(json);
    }

    void startActivity(ActivityId act, Verbosity lvl, ActivityType type,
        const std::string & s, const Fields & fields, ActivityId parent) override
    {
        nlohmann::json json;
        json["action"] = "start";
        json["id"] = act;
        json["level"] = lvl;
        json["type"] = jsonActivityType(type);
        json["text"] = s;
        addFields(json, fields);
        // FIXME: handle parent
        write(json);
    }

    void stopActivity(ActivityId act) override
    {
        nlohmann::json json;
        json["action"] = "stop";
        json["id"] = act;
        write(json);
    }

    void result(ActivityId act, ResultType type, const Fields & fields) override
    {
        nlohmann::json json;
        json["action"] = "result";
        json["id"] = act;
        json["type"] = jsonResultType(type);
        addFields(json, fields);
        write(json);
    }
};

/**
 * A json logger intended for external consumption
 * (contrary to 'JSONLogger' which is an internal thing
 */
struct ExternalJSONLogger : JSONLogger
{
    ExternalJSONLogger(Logger & prevLogger) : JSONLogger(prevLogger) { }

    nlohmann::json jsonActivityType(ActivityType type) override
    {
        switch (type) {
            case actUnknown: return "actUnknown";
            case actCopyPath: return "actCopyPath";
            case actDownload: return "actDownload";
            case actRealise: return "actRealise";
            case actCopyPaths: return "actCopyPaths";
            case actBuilds: return "actBuilds";
            case actBuild: return "actBuild";
            case actOptimiseStore: return "actOptimiseStore";
            case actVerifyPaths: return "actVerifyPaths";
            case actSubstitute: return "actSubstitute";
            case actQueryPathInfo: return "actQueryPathInfo";
            case actPostBuildHook: return "actPostBuildHook";
            default: return "UnknownActivity";
        }
    }

    nlohmann::json jsonResultType(ResultType type) override
    {
        switch (type) {
            case resFileLinked: return "resFileLinked";
            case resBuildLogLine: return "resBuildLogLine";
            case resUntrustedPath: return "resUntrustedPath";
            case resCorruptedPath: return "resCorruptedPath";
            case resSetPhase: return "resSetPhase";
            case resProgress: return "resProgress";
            case resSetExpected: return "resSetExpected";
            case resPostBuildLogLine: return "resPostBuildLogLine";
            default: return "UnknownResultType";
        }
    }

    void result(ActivityId act, ResultType type, const Fields & fields) override {};
};

Logger * makeJSONLogger(Logger & prevLogger)
{
    return new JSONLogger(prevLogger);
}

Logger * makeExternalJSONLogger(Logger & prevLogger)
{
    return new ExternalJSONLogger(prevLogger);
}

static Logger::Fields getFields(nlohmann::json & json)
{
    Logger::Fields fields;
    for (auto & f : json) {
        if (f.type() == nlohmann::json::value_t::number_unsigned)
            fields.emplace_back(Logger::Field(f.get<uint64_t>()));
        else if (f.type() == nlohmann::json::value_t::string)
            fields.emplace_back(Logger::Field(f.get<std::string>()));
        else throw Error("unsupported JSON type %d", (int) f.type());
    }
    return fields;
}

bool handleJSONLogMessage(const std::string & msg,
    const Activity & act, std::map<ActivityId, Activity> & activities, bool trusted)
{
    if (!hasPrefix(msg, "@nix ")) return false;

    try {
        auto json = nlohmann::json::parse(std::string(msg, 5));

        std::string action = json["action"];

        if (action == "start") {
            auto type = (ActivityType) json["type"];
            if (trusted || type == actDownload)
                activities.emplace(std::piecewise_construct,
                    std::forward_as_tuple(json["id"]),
                    std::forward_as_tuple(*logger, (Verbosity) json["level"], type,
                        json["text"], getFields(json["fields"]), act.id));
        }

        else if (action == "stop")
            activities.erase((ActivityId) json["id"]);

        else if (action == "result") {
            auto i = activities.find((ActivityId) json["id"]);
            if (i != activities.end())
                i->second.result((ResultType) json["type"], getFields(json["fields"]));
        }

        else if (action == "setPhase") {
            std::string phase = json["phase"];
            act.result(resSetPhase, phase);
        }

        else if (action == "msg") {
            std::string msg = json["msg"];
            logger->log((Verbosity) json["level"], msg);
        }

    } catch (std::exception & e) {
        printError("bad log message from builder: %s", e.what());
    }

    return true;
}

Activity::~Activity() {
    try {
        logger.stopActivity(id);
    } catch (...) {
        ignoreException();
    }
}

}
