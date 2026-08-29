#pragma once

#include <map>
#include <string>
#include <vector>

class FunctionAST;

using CallGraph = std::map<std::string, std::vector<std::string>>;

CallGraph buildCallGraph(const std::vector<const FunctionAST *> &Functions);
