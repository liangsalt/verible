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

// Run a RuleSet against a VerilogProject and produce structured hits.
// One Hit per (file, rule, CST match) tuple. Bindings expose the captured
// subtrees by name so the daemon RPC can surface them to AI clients.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SCAN_RUNNER_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SCAN_RUNNER_H_

#include <map>
#include <string>
#include <vector>

#include "verible/verilog/analysis/pattern-engine/rule-set.h"

namespace verilog {

// Forward declaration to avoid pulling all of verilog-project.h into clients.
class VerilogProject;

namespace analysis {
namespace pattern_engine {

// A single rule firing on a particular CST subtree of a particular file.
// `bindings` maps the rule's YAML `bind:` keys to byte ranges within the file.
struct Hit {
  std::string rule_name;
  std::string file_path;
  int byte_offset = 0;
  int byte_length = 0;
  std::map<std::string, std::pair<int, int>> bindings;  // name -> [off, len)
};

// Scan every file in `project` against every rule in `rules`. The returned
// vector is grouped first by file, then by rule order in the RuleSet.
//
// Implementation lands in A1.6.
std::vector<Hit> ScanProject(const VerilogProject& project,
                             const RuleSet& rules);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SCAN_RUNNER_H_
