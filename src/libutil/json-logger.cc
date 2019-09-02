#include "json-logger.hh"
#include "logging.hh"
#include "util.hh"

#include <nlohmann/json.hpp>

namespace nix {

JSONLogger::JSONLogger(Logger &prevLogger) : prevLogger(prevLogger) {}

JSONLogger::JSONLogger() : JSONLogger(*makeSimpleLogger()) {}

void JSONLogger::addFields(nlohmann::json &json, const Fields &fields) {
    if (fields.empty())
        return;
    auto &arr = json["fields"] = nlohmann::json::array();
    for (auto &f : fields)
        if (f.type == Logger::Field::tInt)
            arr.push_back(f.i);
        else if (f.type == Logger::Field::tString)
            arr.push_back(f.s);
        else
            abort();
}

void JSONLogger::write(const nlohmann::json &json) {
    prevLogger.log(lvlError, "@nix " + json.dump());
}

void JSONLogger::log(Verbosity lvl, const FormatOrString &fs) {
    nlohmann::json json;
    json["action"] = "msg";
    json["level"] = lvl;
    json["msg"] = fs.s;
    write(json);
}

void JSONLogger::startActivity(ActivityId act, Verbosity lvl, ActivityType type,
                               const std::string &s, const Fields &fields,
                               ActivityId parent) {
    nlohmann::json json;
    json["action"] = "start";
    json["id"] = act;
    json["level"] = lvl;
    json["type"] = type;
    json["text"] = s;
    addFields(json, fields);
    // FIXME: handle parent
    write(json);
}

void JSONLogger::stopActivity(ActivityId act) {
    nlohmann::json json;
    json["action"] = "stop";
    json["id"] = act;
    write(json);
}

void JSONLogger::result(ActivityId act, ResultType type, const Fields &fields) {
    nlohmann::json json;
    json["action"] = "result";
    json["id"] = act;
    json["type"] = type;
    addFields(json, fields);
    write(json);
}

static Logger::Fields getFields(nlohmann::json &json) {
    Logger::Fields fields;
    for (auto &f : json) {
        if (f.type() == nlohmann::json::value_t::number_unsigned)
            fields.emplace_back(Logger::Field(f.get<uint64_t>()));
        else if (f.type() == nlohmann::json::value_t::string)
            fields.emplace_back(Logger::Field(f.get<std::string>()));
        else
            throw Error("unsupported JSON type %d", (int)f.type());
    }
    return fields;
}

bool handleJSONLogMessage(const std::string &msg, const Activity &act,
                          std::map<ActivityId, Activity> &activities,
                          bool trusted) {
    if (!hasPrefix(msg, "@nix "))
        return false;

    try {
        auto json = nlohmann::json::parse(std::string(msg, 5));

        std::string action = json["action"];

        if (action == "start") {
            auto type = (ActivityType)json["type"];
            if (trusted || type == actDownload)
                activities.emplace(
                    std::piecewise_construct, std::forward_as_tuple(json["id"]),
                    std::forward_as_tuple(*logger, (Verbosity)json["level"],
                                          type, json["text"],
                                          getFields(json["fields"]), act.id));
        }

        else if (action == "stop")
            activities.erase((ActivityId)json["id"]);

        else if (action == "result") {
            auto i = activities.find((ActivityId)json["id"]);
            if (i != activities.end())
                i->second.result((ResultType)json["type"],
                                 getFields(json["fields"]));
        }

        else if (action == "setPhase") {
            std::string phase = json["phase"];
            act.result(resSetPhase, phase);
        }

        else if (action == "msg") {
            std::string msg = json["msg"];
            logger->log((Verbosity)json["level"], msg);
        }

    } catch (std::exception &e) {
        printError("bad log message from builder: %s", e.what());
    }

    return true;
}

} // namespace nix
