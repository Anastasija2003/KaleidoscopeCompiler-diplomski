#pragma once

#include <string>
#include <unordered_map>

struct CanonState {
  std::unordered_map<std::string, std::string> Env;
  int NextLocal = 0;
};
