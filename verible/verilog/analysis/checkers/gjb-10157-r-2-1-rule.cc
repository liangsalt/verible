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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-1-rule.h"

#include <cctype>
#include <set>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeLeaf;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(Gjb10157R21Rule);

const LintRuleDescriptor &Gjb10157R21Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-1",
      .topic = "identifiers",
      .desc =
          "Checks that all identifiers (modules, functions, tasks, variables, "
          "signals, packages, classes, interfaces, etc.) start with a letter "
          "and contain only letters, numbers, and underscores. "
          "[GJB 10157 Rule 2-1]",
  };
  return d;
}

absl::Status Gjb10157R21Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

bool Gjb10157R21Rule::IsValidIdentifierName(std::string_view name) {
  if (name.empty()) return false;
  // Must start with a letter
  if (!std::isalpha(static_cast<unsigned char>(name[0]))) return false;
  // Rest must be letters, numbers, or underscores
  for (char c : name) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  return true;
}

void Gjb10157R21Rule::Lint(const verible::TextStructureView &text_structure,
                           std::string_view filename) {
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Helper lambda to check identifier and add violation if invalid
  auto check_identifier = [&](const verible::TokenInfo &token,
                              const verible::SyntaxTreeContext &context,
                              std::string_view kind) {
    std::string_view name = token.text();
    if (!IsValidIdentifierName(name)) {
      std::string reason = absl::StrCat(
          kind, " name '", name,
          "' does not follow naming convention: must start with a letter "
          "and contain only letters, numbers, and underscores. "
          "[GJB 10157 R-2-1]");
      violations_.insert(LintViolation(token, reason, context));
    }
  };

  // Check module names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekModuleDeclaration())) {
    const auto *name_leaf = GetModuleName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Module");
    }
  }

  // Check interface names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekInterfaceDeclaration())) {
    const auto *name_token = GetInterfaceNameToken(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Interface");
    }
  }

  // Check package names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekPackageDeclaration())) {
    const auto *name_token = GetPackageNameToken(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Package");
    }
  }

  // Check function names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekFunctionDeclaration())) {
    const auto *name_leaf = GetFunctionName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Function");
    }
  }

  // Check task names
  for (const auto &match : SearchSyntaxTree(*tree, NodekTaskDeclaration())) {
    const auto *name_leaf = GetTaskName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Task");
    }
  }

  // Check class names
  for (const auto &match : SearchSyntaxTree(*tree, NodekClassDeclaration())) {
    const auto *name_leaf = GetClassName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Class");
    }
  }

  // Check variable/signal names (register variables)
  for (const auto &match : SearchSyntaxTree(*tree, NodekRegisterVariable())) {
    const auto *name_token =
        GetInstanceNameTokenInfoFromRegisterVariable(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Variable");
    }
  }

  // Check instance names (gate instances)
  for (const auto &match : SearchSyntaxTree(*tree, NodekGateInstance())) {
    const auto *name_token =
        GetModuleInstanceNameTokenInfoFromGateInstance(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Instance");
    }
  }
}

verible::LintRuleStatus Gjb10157R21Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
