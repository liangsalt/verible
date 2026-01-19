// Copyright 2024 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_TOP_MODULES_FLAG_H_
#define VERIBLE_VERILOG_ANALYSIS_TOP_MODULES_FLAG_H_

#include <set>
#include <string>
#include <vector>

#include "absl/flags/declare.h"

// Flag for specifying top-level modules for GJB 10157 R-2-10 rule.
// Top-level modules can be identified using:
//   verible-verilog-project top-modules --file_list_path=...
ABSL_DECLARE_FLAG(std::string, top_modules);

namespace verilog {
namespace analysis {

// Global cache for top-level modules detected by Language Server.
// This allows R-2-10 rule to access top modules without FLAGS_top_modules.
class TopModulesCache {
 public:
  static TopModulesCache& GetInstance();

  // Set the list of top-level modules (called by Language Server).
  void SetTopModules(const std::vector<std::string>& modules);

  // Get the set of top-level modules.
  const std::set<std::string>& GetTopModules() const;

  // Check if any top modules have been set.
  bool HasTopModules() const;

  // Clear the cache.
  void Clear();

 private:
  TopModulesCache() = default;
  std::set<std::string> top_modules_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_TOP_MODULES_FLAG_H_
