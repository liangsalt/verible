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

#include "verible/verilog/analysis/top-modules-flag.h"

#include <set>
#include <string>
#include <vector>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, top_modules, "",
          "Comma-separated list of top-level module names. "
          "Used by GJB 10157 R-2-10 rule to check for floating inputs. "
          "Use verible-verilog-project top-modules command to identify these.");

namespace verilog {
namespace analysis {

TopModulesCache& TopModulesCache::GetInstance() {
  static TopModulesCache instance;
  return instance;
}

void TopModulesCache::SetTopModules(const std::vector<std::string>& modules) {
  top_modules_.clear();
  for (const auto& m : modules) {
    top_modules_.insert(m);
  }
}

const std::set<std::string>& TopModulesCache::GetTopModules() const {
  return top_modules_;
}

bool TopModulesCache::HasTopModules() const {
  return !top_modules_.empty();
}

void TopModulesCache::Clear() {
  top_modules_.clear();
}

}  // namespace analysis
}  // namespace verilog
