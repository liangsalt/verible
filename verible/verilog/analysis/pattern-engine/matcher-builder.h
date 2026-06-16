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

// Name -> enum registries used by the YAML rule loader.
// YAML rules reference CST/token nodes by string name (e.g. "kBinaryExpression",
// "SymbolIdentifier"); this header exposes lookup tables that translate those
// strings into the matching NodeEnum / verilog_tokentype values at runtime.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_MATCHER_BUILDER_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_MATCHER_BUILDER_H_

#include <optional>
#include <string_view>

namespace verilog {
namespace analysis {
namespace pattern_engine {

// Look up a Verilog CST NodeEnum value by its string name (without the leading
// "k", or with -- both forms accepted). Returns nullopt if unknown.
// Backed by the verilog_nonterminals_foreach.inc X-macro.
//
// Implementation lands in A1.3.
std::optional<int> LookupNodeEnum(std::string_view name);

// Look up a Verilog token enum value by its string name.
// Backed by verilog-token-enum.h.
//
// Implementation lands in A1.3.
std::optional<int> LookupTokenEnum(std::string_view name);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_MATCHER_BUILDER_H_
