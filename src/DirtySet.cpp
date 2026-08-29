#include "DirtySet.h"

#include <deque>
#include <vector>

std::set<std::string>
computeDirtySet(const std::map<std::string, std::string> &NewHashes,
                 const CallGraph &NewCallGraph, const FunctionCache &OldCache) {
  std::set<std::string> Dirty;
  std::deque<std::string> Queue;

  const auto &OldFunctions = OldCache.getFunctions();
  for (auto &[Name, Hash] : NewHashes) {
    auto It = OldFunctions.find(Name);
    bool Changed = It == OldFunctions.end() || It->second.Hash != Hash;
    if (Changed && Dirty.insert(Name).second)
      Queue.push_back(Name);
  }

  std::map<std::string, std::vector<std::string>> Callers;
  for (auto &[Caller, Callees] : NewCallGraph)
    for (auto &Callee : Callees)
      Callers[Callee].push_back(Caller);

  while (!Queue.empty()) {
    std::string Name = Queue.front();
    Queue.pop_front();

    auto It = Callers.find(Name);
    if (It == Callers.end())
      continue;

    for (auto &Caller : It->second)
      if (Dirty.insert(Caller).second)
        Queue.push_back(Caller);
  }

  return Dirty;
}
