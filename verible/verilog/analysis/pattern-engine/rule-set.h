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

// Holds a workspace's compiled YAML rules merged across the four scopes
// (project > user > team > community). Same-name rules in higher-precedence
// scopes shadow lower ones. Designed to be reloaded in place on rules/
// directory changes; the daemon's verilog/debug/reloadRules RPC drives this.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_RULE_SET_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_RULE_SET_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

// Filesystem paths to scan for *.yaml rules, one per scope.
// Any scope with an empty path is skipped.
struct ScopePaths {
  std::string project_dir;    // <project_root>/rules/
  std::string user_dir;       // ~/.duducode/rtl-brain/rules/user/
  std::string team_dir;       // ~/.duducode/teams/<id>/rtl-rules/
  std::string community_dir;  // ~/.duducode/rtl-brain/rules/community/
};

class RuleSet {
 public:
  RuleSet() = default;

  // Load (or reload) every *.yaml under the given scope directories. Rules in
  // higher-precedence scopes shadow same-named rules in lower scopes. Returns
  // OK even if individual files fail to parse; per-file errors are collected.
  //
  // Implementation lands in A1.5.
  absl::Status LoadFromScopes(const ScopePaths& paths);

  // Reload only the named scope (e.g. user edited a project YAML). Cheaper
  // than a full reload but still rebuilds matcher trees for the affected rules.
  absl::Status ReloadScope(RuleScope scope, std::string_view dir);

  // All rules in precedence-resolved order.
  const std::vector<LoadedRule>& rules() const { return active_rules_; }

  // Per-file errors collected on the last load. Empty on a clean load.
  const std::vector<std::string>& load_errors() const { return load_errors_; }

 private:
  std::vector<LoadedRule> active_rules_;
  std::vector<std::string> load_errors_;
};

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_RULE_SET_H_
