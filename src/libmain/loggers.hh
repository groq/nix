#pragma once

#include "logging.hh"
#include "types.hh"

namespace nix {

enum class LogFormat {
  raw,
  rawWithLogs,
  internalJson,
  bar,
  barWithLogs,
};

void setLogFormat(const string &logFormatStr);
void setLogFormat(const LogFormat &logFormat);

void createDefaultLogger();

bool handleJSONLogMessage(const std::string &msg, const Activity &act,
                          std::map<ActivityId, Activity> &activities,
                          bool trusted);

}
