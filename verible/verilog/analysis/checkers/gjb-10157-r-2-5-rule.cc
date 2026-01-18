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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-5-rule.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(Gjb10157R25Rule);

const LintRuleDescriptor &Gjb10157R25Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-5",
      .topic = "file-structure",
      .desc =
          "Checks that each file contains only one module declaration. "
          "[GJB 10157 Rule 2-5]",
  };
  return d;
}

absl::Status Gjb10157R25Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

void Gjb10157R25Rule::Lint(const verible::TextStructureView &text_structure,
                           std::string_view filename) {
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all module declarations
  auto module_matches = FindAllModuleDeclarations(*tree);
  if (module_matches.empty()) {
    return;
  }

  // Remove nested module declarations (they are allowed within outer modules)
  std::vector<verible::TreeSearchMatch> module_cleaned;
  module_cleaned.reserve(module_matches.size());
  std::back_insert_iterator<std::vector<verible::TreeSearchMatch>> back_it(
      module_cleaned);
  std::remove_copy_if(module_matches.begin(), module_matches.end(), back_it,
                      [](verible::TreeSearchMatch &m) {
                        return m.context.IsInside(NodeEnum::kModuleDeclaration);
                      });

  // If more than one top-level module, report violations for each extra module
  if (module_cleaned.size() > 1) {
    for (size_t i = 1; i < module_cleaned.size(); ++i) {
      const auto *module_name = GetModuleName(*module_cleaned[i].match);
      if (module_name != nullptr) {
        std::string reason = absl::StrCat(
            "Multiple modules in one file: module '", module_name->get().text(),
            "' is the ", (i + 1), "th module in this file. "
            "Each file should contain only one module. [GJB 10157 R-2-5]");
        violations_.insert(
            LintViolation(module_name->get(), reason, module_cleaned[i].context));
      }
    }
  }
}

verible::LintRuleStatus Gjb10157R25Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
