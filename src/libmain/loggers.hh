#pragma once

#include "logging.hh"
#include "types.hh"

namespace nix {

bool handleJSONLogMessage(const std::string &msg, const Activity &act,
                          std::map<ActivityId, Activity> &activities,
                          bool trusted);

}
