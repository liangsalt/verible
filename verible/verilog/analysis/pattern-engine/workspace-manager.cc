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

#include "verible/verilog/analysis/pattern-engine/workspace-manager.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

absl::Status WorkspaceManager::Open(
    std::string_view root, const ScopePaths& scope_paths,
    const std::vector<std::string>& extra_source_dirs) {
  const std::string key(root);
  if (workspaces_.find(key) != workspaces_.end()) {
    return absl::AlreadyExistsError(
        absl::StrCat("workspace already open: ", key));
  }

  // Validate root exists. Daemon clients are expected to pass absolute paths.
  std::error_code ec;
  if (!std::filesystem::exists(key, ec) ||
      !std::filesystem::is_directory(key, ec)) {
    return absl::InvalidArgumentError(
        absl::StrCat("workspace root does not exist or is not a directory: ",
                     key));
  }

  WorkspaceState state;
  state.root = key;
  state.scope_paths = scope_paths;
  state.project = std::make_unique<VerilogProject>(
      /*root=*/key,
      /*include_paths=*/extra_source_dirs);

  auto status = state.rule_set.LoadFromScopes(scope_paths);
  if (!status.ok()) return status;

  workspaces_.emplace(key, std::move(state));
  return absl::OkStatus();
}

absl::Status WorkspaceManager::Close(std::string_view root) {
  const std::string key(root);
  auto it = workspaces_.find(key);
  if (it == workspaces_.end()) {
    return absl::NotFoundError(absl::StrCat("no such workspace: ", key));
  }
  workspaces_.erase(it);
  return absl::OkStatus();
}

WorkspaceState* WorkspaceManager::Get(std::string_view root) {
  auto it = workspaces_.find(std::string(root));
  return it == workspaces_.end() ? nullptr : &it->second;
}

const WorkspaceState* WorkspaceManager::Get(std::string_view root) const {
  auto it = workspaces_.find(std::string(root));
  return it == workspaces_.end() ? nullptr : &it->second;
}

absl::Status WorkspaceManager::ReloadRules(std::string_view root) {
  WorkspaceState* w = Get(root);
  if (w == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("workspace not open: ", std::string(root)));
  }
  return w->rule_set.LoadFromScopes(w->scope_paths);
}

std::vector<std::string> WorkspaceManager::ListOpenWorkspaces() const {
  std::vector<std::string> out;
  out.reserve(workspaces_.size());
  for (const auto& [root, _] : workspaces_) out.push_back(root);
  return out;
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
