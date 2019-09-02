#include "logging.hh"
#include "util.hh"

#include <atomic>
#include <nlohmann/json.hpp>
#include <iostream>

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

void Logger::warn(const std::string & msg)
{
    log(lvlWarn, ANSI_RED "warning:" ANSI_NORMAL " " + msg);
}

void Logger::writeToStdout(std::string_view s)
{
    std::cout << s << "\n";
}

class SimpleLogger : public Logger
{
public:

    bool systemd, tty;
    bool printBuildLogs;

    SimpleLogger(bool printBuildLogs)
        : printBuildLogs(printBuildLogs)
    {
        systemd = getEnv("IN_SYSTEMD") == "1";
        tty = isatty(STDERR_FILENO);
    }

    bool isVerbose() override {
        return printBuildLogs;
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

    void result(ActivityId act, ResultType type, const Fields & fields) override
    {
        if (type == resBuildLogLine && printBuildLogs) {
            auto lastLine = fields[0].s;
            printError(lastLine);
        }
        else if (type == resPostBuildLogLine && printBuildLogs) {
            auto lastLine = fields[0].s;
            printError("post-build-hook: " + lastLine);
        }
    }
};

Logger * makeSimpleLogger(bool printBuildLogs)
{
    return new SimpleLogger(printBuildLogs);
}

Logger *logger = makeSimpleLogger(true);

Verbosity verbosity = lvlInfo;

void warnOnce(bool &haveWarned, const FormatOrString &fs) {
    if (!haveWarned) {
        warn(fs.s);
        haveWarned = true;
    }
}

void writeToStderr(const string &s) {
    try {
        writeFull(STDERR_FILENO, s, false);
    } catch (SysError &e) {
        /* Ignore failing writes to stderr.  We need to ignore write
       errors to ensure that cleanup code that logs to stderr runs
       to completion if the other side of stderr has been closed
       unexpectedly. */
    }
}

std::atomic<uint64_t> nextId{(uint64_t)getpid() << 32};

Activity::Activity(Logger &logger, Verbosity lvl, ActivityType type,
                   const std::string &s, const Logger::Fields &fields,
                   ActivityId parent)
    : logger(logger), id(nextId++) {
    logger.startActivity(id, lvl, type, s, fields, parent);
}

Activity::~Activity() {
    try {
        logger.stopActivity(id);
    } catch (...) {
        ignoreException();
    }
}

} // namespace nix
