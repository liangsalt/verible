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

#include "verible/verilog/analysis/pattern-engine/rule-set.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

// Load every *.yaml file in `dir` at the given scope. Collects load errors
// per-file instead of failing whole loads. Files that fail to parse are
// skipped but reported.
void LoadDir(const std::string& dir, RuleScope scope,
             std::vector<LoadedRule>* out,
             std::vector<std::string>* errors) {
  if (dir.empty()) return;
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) ||
      !std::filesystem::is_directory(dir, ec)) {
    return;  // Missing directory is not an error -- scope simply has no rules.
  }
  for (const auto& entry :
       std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      errors->push_back(
          absl::StrCat("directory iterate failed: ", dir, ": ", ec.message()));
      return;
    }
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".yaml" &&
        entry.path().extension() != ".yml") {
      continue;
    }
    const std::string file_path = entry.path().string();
    auto rule = LoadRuleFromYaml(file_path, scope);
    if (!rule.ok()) {
      errors->push_back(
          absl::StrCat(file_path, ": ", rule.status().message()));
      continue;
    }
    out->push_back(std::move(*rule));
  }
}

}  // namespace

absl::Status RuleSet::LoadFromScopes(const ScopePaths& paths) {
  active_rules_.clear();
  load_errors_.clear();

  // Load lowest precedence first; higher scopes will overwrite same-name rules.
  std::vector<LoadedRule> staged;
  LoadDir(paths.community_dir, RuleScope::kCommunity, &staged, &load_errors_);
  LoadDir(paths.team_dir, RuleScope::kTeam, &staged, &load_errors_);
  LoadDir(paths.user_dir, RuleScope::kUser, &staged, &load_errors_);
  LoadDir(paths.project_dir, RuleScope::kProject, &staged, &load_errors_);

  // Merge by name with higher-precedence (later in load order) winning.
  // RuleScope is ordered low value = highest precedence, but our load order
  // is reverse so latest-wins naturally yields project > user > team > community.
  absl::flat_hash_map<std::string, size_t> name_to_index;
  for (size_t i = 0; i < staged.size(); ++i) {
    name_to_index[staged[i].name] = i;
  }
  active_rules_.reserve(name_to_index.size());
  for (auto& [name, idx] : name_to_index) {
    active_rules_.push_back(std::move(staged[idx]));
  }
  return absl::OkStatus();
}

absl::Status RuleSet::ReloadScope(RuleScope scope, std::string_view dir) {
  // Naive impl: drop rules from this scope and re-walk the directory.
  // A1.5 keeps it simple; later we can hot-reload individual files.
  active_rules_.erase(
      std::remove_if(active_rules_.begin(), active_rules_.end(),
                     [scope](const LoadedRule& r) {
                       return r.source_scope == scope;
                     }),
      active_rules_.end());

  std::vector<LoadedRule> staged;
  LoadDir(std::string(dir), scope, &staged, &load_errors_);
  for (auto& r : staged) {
    // If a higher-precedence scope already has the same name, skip.
    bool shadowed = false;
    for (const auto& existing : active_rules_) {
      if (existing.name == r.name &&
          static_cast<int>(existing.source_scope) <
              static_cast<int>(scope)) {
        shadowed = true;
        break;
      }
    }
    if (!shadowed) active_rules_.push_back(std::move(r));
  }
  return absl::OkStatus();
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
