#include <catch2/catch_test_macros.hpp>

#include "FunctionCache.h"

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path tempDir() {
  auto Dir = std::filesystem::temp_directory_path() / "kalcache_test";
  std::filesystem::remove_all(Dir);
  return Dir;
}

} // namespace

TEST_CASE("load returns an empty cache when no manifest exists", "[cache]") {
  auto Dir = tempDir();
  FunctionCache Cache = FunctionCache::load(Dir.string());
  REQUIRE(Cache.getFunctions().empty());
}

TEST_CASE("save then load round-trips function entries", "[cache]") {
  auto Dir = tempDir();

  FunctionCache Cache;
  Cache.setVersionInfo("21.1.8", "arm64-apple-darwin24.6.0");
  FunctionCacheEntry Entry;
  Entry.Hash = "abc123";
  Entry.Callees = {"bar", "baz"};
  Entry.BcPath = "foo.bc";
  Entry.OPath = "foo.o";
  Cache.getFunctions()["foo"] = Entry;
  Cache.save(Dir.string());

  FunctionCache Loaded = FunctionCache::load(Dir.string());
  REQUIRE(Loaded.getFunctions().size() == 1);
  REQUIRE(Loaded.getFunctions().at("foo").Hash == "abc123");
  REQUIRE(Loaded.getFunctions().at("foo").Callees ==
          std::vector<std::string>{"bar", "baz"});
  REQUIRE(Loaded.getFunctions().at("foo").BcPath == "foo.bc");
  REQUIRE(Loaded.getFunctions().at("foo").OPath == "foo.o");
  REQUIRE(Loaded.isCompatible("21.1.8", "arm64-apple-darwin24.6.0"));

  std::filesystem::remove_all(Dir);
}

TEST_CASE("isCompatible detects LLVM version/target mismatch", "[cache]") {
  auto Dir = tempDir();

  FunctionCache Cache;
  Cache.setVersionInfo("21.1.8", "arm64-apple-darwin24.6.0");
  Cache.save(Dir.string());

  FunctionCache Loaded = FunctionCache::load(Dir.string());
  REQUIRE_FALSE(Loaded.isCompatible("20.0.0", "arm64-apple-darwin24.6.0"));
  REQUIRE_FALSE(Loaded.isCompatible("21.1.8", "x86_64-pc-linux-gnu"));

  std::filesystem::remove_all(Dir);
}

TEST_CASE("load falls back to an empty cache on a corrupted manifest",
          "[cache]") {
  auto Dir = tempDir();
  std::filesystem::create_directories(Dir);
  std::ofstream Out(Dir / "manifest.json");
  Out << "{ not valid json";
  Out.close();

  FunctionCache Cache = FunctionCache::load(Dir.string());
  REQUIRE(Cache.getFunctions().empty());

  std::filesystem::remove_all(Dir);
}
