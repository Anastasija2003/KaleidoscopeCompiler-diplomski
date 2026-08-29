#include "FunctionCache.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

FunctionCache FunctionCache::load(const std::string &Dir) {
  FunctionCache Cache;

  std::ifstream In(Dir + "/manifest.json");
  if (!In)
    return Cache;

  try {
    nlohmann::json J;
    In >> J;

    Cache.LLVMVersion = J.value("llvm_version", "");
    Cache.TargetTriple = J.value("target_triple", "");

    for (auto &[Name, EntryJson] : J.at("functions").items()) {
      FunctionCacheEntry Entry;
      Entry.Hash = EntryJson.at("hash").get<std::string>();
      Entry.Callees = EntryJson.at("callees").get<std::vector<std::string>>();
      Entry.BcPath = EntryJson.at("bc").get<std::string>();
      Entry.OPath = EntryJson.at("o").get<std::string>();
      Cache.Functions[Name] = std::move(Entry);
    }
  } catch (const nlohmann::json::exception &) {
    return FunctionCache();
  }

  return Cache;
}

void FunctionCache::save(const std::string &Dir) const {
  std::filesystem::create_directories(Dir);

  nlohmann::json J;
  J["llvm_version"] = LLVMVersion;
  J["target_triple"] = TargetTriple;

  nlohmann::json FunctionsJson = nlohmann::json::object();
  for (auto &[Name, Entry] : Functions) {
    FunctionsJson[Name] = {
        {"hash", Entry.Hash},
        {"callees", Entry.Callees},
        {"bc", Entry.BcPath},
        {"o", Entry.OPath},
    };
  }
  J["functions"] = std::move(FunctionsJson);

  std::ofstream Out(Dir + "/manifest.json");
  Out << J.dump(2);
}

bool FunctionCache::isCompatible(const std::string &LLVMVer,
                                  const std::string &Triple) const {
  return LLVMVersion == LLVMVer && TargetTriple == Triple;
}

void FunctionCache::setVersionInfo(std::string LLVMVer, std::string Triple) {
  LLVMVersion = std::move(LLVMVer);
  TargetTriple = std::move(Triple);
}
