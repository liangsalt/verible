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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_GJB_10157_R_2_3_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_GJB_10157_R_2_3_RULE_H_

#include <set>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Gjb10157R23Rule enforces GJB 10157 Rule 2-3:
// Identifiers must not be the same as keywords from Verilog, SystemVerilog,
// VHDL, SDF, or EDIF (case-insensitive comparison).
class Gjb10157R23Rule : public verible::TextStructureLintRule {
 public:
  using rule_type = verible::TextStructureLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  absl::Status Configure(std::string_view configuration) final;
  void Lint(const verible::TextStructureView &,
            std::string_view filename) final;

  verible::LintRuleStatus Report() const final;

 private:
  // Check if the identifier matches any reserved keyword (case-insensitive).
  // Returns the language name if it's a keyword, or empty string if not.
  static std::string IsReservedKeyword(std::string_view name);

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_GJB_10157_R_2_3_RULE_H_
