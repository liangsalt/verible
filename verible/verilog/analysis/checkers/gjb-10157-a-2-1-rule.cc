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

#include "verible/verilog/analysis/checkers/gjb-10157-a-2-1-rule.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/file-util.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::TextStructureView;

VERILOG_REGISTER_LINT_RULE(Gjb10157A21Rule);

const LintRuleDescriptor& Gjb10157A21Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-A-2-1",
      .topic = "file-names",
      .desc =
          "[Advisory] Filename should match module name. "
          "[GJB 10157 A-2-1]",
      .param = {},
      .severity = LintRuleSeverity::kWarning,  // Advisory rule = warning (yellow)
  };
  return d;
}

absl::Status Gjb10157A21Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

static bool ModuleNameMatches(const verible::Symbol& s, std::string_view name) {
  const auto* module_leaf = GetModuleName(s);
  return module_leaf && module_leaf->get().text() == name;
}

void Gjb10157A21Rule::Lint(const TextStructureView& text_structure,
                           std::string_view filename) {
  if (verible::file::IsStdin(filename)) {
    return;
  }

  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all module declarations.
  auto module_matches = FindAllModuleDeclarations(*tree);

  // If there are no modules in this source unit, suppress finding.
  if (module_matches.empty()) return;

  // Remove nested module declarations
  std::vector<verible::TreeSearchMatch> module_cleaned;
  module_cleaned.reserve(module_matches.size());
  std::back_insert_iterator<std::vector<verible::TreeSearchMatch>> back_it(
      module_cleaned);
  std::remove_copy_if(module_matches.begin(), module_matches.end(), back_it,
                      [](verible::TreeSearchMatch& m) {
                        return m.context.IsInside(NodeEnum::kModuleDeclaration);
                      });

  // See if any names match the stem of the filename.
  const std::string_view basename = verible::file::Basename(filename);

  std::vector<std::string_view> basename_components =
      absl::StrSplit(basename, '.');
  std::string unitname(basename_components[0].begin(),
                       basename_components[0].end());
  if (unitname.empty()) return;

  // If there is at least one module with a matching name, suppress finding.
  if (std::any_of(module_cleaned.begin(), module_cleaned.end(),
                  [=](const verible::TreeSearchMatch& m) {
                    return ModuleNameMatches(*m.match, unitname);
                  })) {
    return;
  }

  // Only report a violation on the last module declaration.
  const verible::Symbol& last_module = *module_cleaned.back().match;
  const auto* last_module_id = GetModuleName(last_module);
  if (last_module_id) {
    std::string reason = absl::StrCat(
        "Filename '", unitname,
        "' does not match module name '", last_module_id->get().text(),
        "'. Suggest renaming file or module. [GJB 10157 A-2-1]");
    violations_.insert(verible::LintViolation(last_module_id->get(), reason));
  }
}

verible::LintRuleStatus Gjb10157A21Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
