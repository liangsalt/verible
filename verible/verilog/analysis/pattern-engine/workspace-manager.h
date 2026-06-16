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

// Multi-workspace state for the daemon. One WorkspaceState per project root;
// each owns its own VerilogProject + RuleSet. The daemon's RPC dispatcher
// looks up WorkspaceState by root path; clients reference workspaces by their
// root path string in every RPC payload.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_WORKSPACE_MANAGER_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_WORKSPACE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

// State owned per project root opened by the daemon.
//
// VerilogProject is non-movable, so we store it via unique_ptr to keep
// WorkspaceState movable.
struct WorkspaceState {
  std::string root;  // absolute path to project root
  std::unique_ptr<VerilogProject> project;
  RuleSet rule_set;
  ScopePaths scope_paths;
};

class WorkspaceManager {
 public:
  WorkspaceManager() = default;

  // Open a workspace. Loads YAML rules from the four scope directories.
  // `extra_source_dirs` is optional and lets the project pick up RTL files
  // from include paths beyond `root`. Returns AlreadyExists if the workspace
  // is already open.
  absl::Status Open(std::string_view root, const ScopePaths& scope_paths,
                    const std::vector<std::string>& extra_source_dirs = {});

  // Close a previously opened workspace. NotFound if root is unknown.
  absl::Status Close(std::string_view root);

  // Get pointer to workspace state. Returns nullptr if not open.
  // The returned pointer is owned by this manager; do not free.
  WorkspaceState* Get(std::string_view root);
  const WorkspaceState* Get(std::string_view root) const;

  // Reload only the rule_set of a workspace. Useful after AI writes a new
  // YAML and wants to make it active immediately.
  absl::Status ReloadRules(std::string_view root);

  // List currently open workspace root paths (for debugging / diagnostics).
  std::vector<std::string> ListOpenWorkspaces() const;

 private:
  std::map<std::string, WorkspaceState> workspaces_;
};

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_WORKSPACE_MANAGER_H_
