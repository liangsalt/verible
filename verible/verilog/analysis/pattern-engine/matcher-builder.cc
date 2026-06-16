// Copyright 2026 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verible/verilog/analysis/pattern-engine/matcher-builder.h"

#include <optional>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

// Built once at first call. Two entries per NodeEnum: with and without the
// leading 'k' (YAML rules often drop the k for readability).
const absl::flat_hash_map<std::string, int>& NodeEnumTable() {
  static const auto* const table = []() {
    auto* m = new absl::flat_hash_map<std::string, int>;
#define CONSIDER(val)                                       \
  do {                                                      \
    const int v = static_cast<int>(verilog::NodeEnum::val); \
    (*m)[#val] = v;                                         \
    const std::string_view name_with_k(#val);               \
    if (!name_with_k.empty() && name_with_k.front() == 'k') {        \
      (*m)[std::string(name_with_k.substr(1))] = v;         \
    }                                                       \
  } while (false);
#include "verible/verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep
#undef CONSIDER
    return m;
  }();
  return *table;
}

}  // namespace

std::optional<int> LookupNodeEnum(std::string_view name) {
  const auto& table = NodeEnumTable();
  // flat_hash_map::find via std::string_view is supported when key is std::string
  // with C++17 heterogeneous lookup; fall back to copy to be safe.
  auto it = table.find(std::string(name));
  if (it == table.end()) return std::nullopt;
  return it->second;
}

std::optional<int> LookupTokenEnum(std::string_view /*name*/) {
  // Tokens in YAML rules are matched by literal text (e.g. "==", "always_ff")
  // rather than by yytokentype enum, because verilog-token-enum.h is generated
  // at build time and not amenable to X-macro enumeration here. yaml-rule-loader
  // emits LeafTag matchers using the textual form directly.
  return std::nullopt;
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
