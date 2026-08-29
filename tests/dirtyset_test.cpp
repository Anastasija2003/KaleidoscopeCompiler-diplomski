#include <catch2/catch_test_macros.hpp>

#include "DirtySet.h"

namespace {

FunctionCache makeCache(const std::map<std::string, std::string> &Hashes) {
  FunctionCache Cache;
  for (auto &[Name, Hash] : Hashes) {
    FunctionCacheEntry Entry;
    Entry.Hash = Hash;
    Cache.getFunctions()[Name] = Entry;
  }
  return Cache;
}

} // namespace

TEST_CASE("unchanged function with unchanged callers stays clean",
          "[dirtyset]") {
  FunctionCache Old = makeCache({{"a", "hashA"}, {"b", "hashB"}});
  std::map<std::string, std::string> NewHashes = {{"a", "hashA"},
                                                    {"b", "hashB"}};
  CallGraph Graph = {{"a", {"b"}}, {"b", {}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty.empty());
}

TEST_CASE("changing a leaf function invalidates only that function",
          "[dirtyset]") {
  FunctionCache Old = makeCache({{"a", "hashA"}, {"leaf", "hashLeaf"}});
  std::map<std::string, std::string> NewHashes = {{"a", "hashA"},
                                                    {"leaf", "hashLeafChanged"}};
  CallGraph Graph = {{"a", {}}, {"leaf", {}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty == std::set<std::string>{"leaf"});
}

TEST_CASE("changing a hub function invalidates all its callers",
          "[dirtyset]") {
  FunctionCache Old =
      makeCache({{"hub", "h1"}, {"a", "ha"}, {"b", "hb"}, {"c", "hc"}});
  std::map<std::string, std::string> NewHashes = {
      {"hub", "h2"}, {"a", "ha"}, {"b", "hb"}, {"c", "hc"}};
  CallGraph Graph = {
      {"a", {"hub"}}, {"b", {"hub"}}, {"c", {"hub"}}, {"hub", {}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty == std::set<std::string>{"hub", "a", "b", "c"});
}

TEST_CASE("dirty propagates transitively through a chain", "[dirtyset]") {
  FunctionCache Old = makeCache({{"a", "ha"}, {"b", "hb"}, {"c", "hc"}});
  std::map<std::string, std::string> NewHashes = {
      {"a", "ha"}, {"b", "hb"}, {"c", "hcChanged"}};
  CallGraph Graph = {{"a", {"b"}}, {"b", {"c"}}, {"c", {}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty == std::set<std::string>{"a", "b", "c"});
}

TEST_CASE("a new function not present in the old cache is dirty",
          "[dirtyset]") {
  FunctionCache Old = makeCache({{"a", "ha"}});
  std::map<std::string, std::string> NewHashes = {{"a", "ha"},
                                                    {"brandNew", "hn"}};
  CallGraph Graph = {{"a", {}}, {"brandNew", {}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty == std::set<std::string>{"brandNew"});
}

TEST_CASE("mutual recursion does not infinite-loop and both become dirty",
          "[dirtyset]") {
  FunctionCache Old = makeCache({{"a", "ha"}, {"b", "hb"}});
  std::map<std::string, std::string> NewHashes = {{"a", "haChanged"},
                                                    {"b", "hb"}};
  CallGraph Graph = {{"a", {"b"}}, {"b", {"a"}}};

  auto Dirty = computeDirtySet(NewHashes, Graph, Old);
  REQUIRE(Dirty == std::set<std::string>{"a", "b"});
}
