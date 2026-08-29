#pragma once

#include <map>
#include <string>
#include <vector>

struct FunctionCacheEntry {
  std::string Hash;
  std::vector<std::string> Callees;
  std::string BcPath;
  std::string OPath;
};

class FunctionCache {
public:
  static FunctionCache load(const std::string &Dir);
  void save(const std::string &Dir) const;

  bool isCompatible(const std::string &LLVMVersion,
                     const std::string &TargetTriple) const;
  void setVersionInfo(std::string LLVMVersion, std::string TargetTriple);

  const std::map<std::string, FunctionCacheEntry> &getFunctions() const {
    return Functions;
  }
  std::map<std::string, FunctionCacheEntry> &getFunctions() { return Functions; }

private:
  std::string LLVMVersion;
  std::string TargetTriple;
  std::map<std::string, FunctionCacheEntry> Functions;
};
