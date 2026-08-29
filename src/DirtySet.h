#pragma once

#include "CallGraph.h"
#include "FunctionCache.h"

#include <map>
#include <set>
#include <string>

std::set<std::string>
computeDirtySet(const std::map<std::string, std::string> &NewHashes,
                 const CallGraph &NewCallGraph, const FunctionCache &OldCache);
