#pragma once

#include "logging.hh"

namespace nix {

Logger* createProgressBar(bool printBuildLogs = false);

void startProgressBar(bool printBuildLogs = false);

void stopProgressBar();

}
