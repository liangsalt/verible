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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-10-rule.h"

#include <map>
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
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;

VERILOG_REGISTER_LINT_RULE(Gjb10157R210Rule);

const LintRuleDescriptor& Gjb10157R210Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-10",
      .topic = "port-usage",
      .desc =
          "Top-level input ports must not be left floating (unused). "
          "[GJB 10157 Rule 2-10]",
  };
  return d;
}

absl::Status Gjb10157R210Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

// Collect all SymbolIdentifier tokens in a subtree
static void CollectAllIdentifiers(const verible::Symbol& root,
                                   std::set<std::string_view>& identifiers) {
  if (root.Kind() == verible::SymbolKind::kLeaf) {
    const auto& leaf = verible::SymbolCastToLeaf(root);
    if (leaf.get().token_enum() == SymbolIdentifier) {
      identifiers.insert(leaf.get().text());
    }
    return;
  }
  
  const auto& node = verible::SymbolCastToNode(root);
  for (const auto& child : node.children()) {
    if (child != nullptr) {
      CollectAllIdentifiers(*child, identifiers);
    }
  }
}

void Gjb10157R210Rule::Lint(const verible::TextStructureView& text_structure,
                             std::string_view filename) {
  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all module declarations
  auto module_matches = FindAllModuleDeclarations(*tree);
  
  for (const auto& module_match : module_matches) {
    const verible::Symbol* module_symbol = module_match.match;
    if (module_symbol == nullptr) continue;
    
    // Find all port declarations in this module
    auto port_matches = FindAllModulePortDeclarations(*module_symbol);
    
    // Collect input port names and their tokens
    std::map<std::string_view, const verible::TokenInfo*> input_ports;
    
    for (const auto& port_match : port_matches) {
      const auto* direction_leaf = 
          GetDirectionFromModulePortDeclaration(*port_match.match);
      if (direction_leaf == nullptr) continue;
      
      // Check if it's an input port
      std::string_view direction = direction_leaf->get().text();
      if (direction != "input") continue;
      
      // Get the port identifier
      const auto* id_leaf = 
          GetIdentifierFromModulePortDeclaration(*port_match.match);
      if (id_leaf == nullptr) continue;
      
      input_ports[id_leaf->get().text()] = &id_leaf->get();
    }
    
    if (input_ports.empty()) continue;
    
    // Collect all identifier references in the module body
    std::set<std::string_view> used_identifiers;
    CollectAllIdentifiers(*module_symbol, used_identifiers);
    
    // Check each input port - it should be referenced more than once
    // (once in declaration, at least once in usage)
    for (const auto& [port_name, token] : input_ports) {
      // Count occurrences of this identifier
      int count = 0;
      for (const auto& id : used_identifiers) {
        if (id == port_name) count++;
      }
      
      // If only found in declaration (count <= 1), it's unused
      // We use a simpler heuristic: check if it appears in used_identifiers
      // but since we're collecting all, we need to be smarter
      // Actually, used_identifiers is a set, so count is always 0 or 1
      // Let's recollect with counting
    }
  }
  
  // Simplified approach: For each module, find inputs and check if they
  // appear in expressions (not just declarations)
  for (const auto& module_match : module_matches) {
    const verible::Symbol* module_symbol = module_match.match;
    if (module_symbol == nullptr) continue;
    
    auto port_matches = FindAllModulePortDeclarations(*module_symbol);
    
    struct InputPortInfo {
      const verible::TokenInfo* token;
      bool used;
    };
    std::map<std::string_view, InputPortInfo> input_ports;
    
    for (const auto& port_match : port_matches) {
      const auto* direction_leaf = 
          GetDirectionFromModulePortDeclaration(*port_match.match);
      if (direction_leaf == nullptr) continue;
      
      if (direction_leaf->get().text() != "input") continue;
      
      const auto* id_leaf = 
          GetIdentifierFromModulePortDeclaration(*port_match.match);
      if (id_leaf == nullptr) continue;
      
      input_ports[id_leaf->get().text()] = {&id_leaf->get(), false};
    }
    
    if (input_ports.empty()) continue;
    
    // Find all symbol identifier references in the module
    auto id_matches = SearchSyntaxTree(*module_symbol, NodekReference());
    
    for (const auto& id_match : id_matches) {
      const auto* leftmost = verible::GetLeftmostLeaf(*id_match.match);
      if (leftmost != nullptr && 
          leftmost->get().token_enum() == SymbolIdentifier) {
        auto it = input_ports.find(leftmost->get().text());
        if (it != input_ports.end()) {
          it->second.used = true;
        }
      }
    }
    
    // Also check in expressions
    auto expr_matches = SearchSyntaxTree(*module_symbol, NodekExpression());
    for (const auto& expr_match : expr_matches) {
      std::set<std::string_view> expr_ids;
      CollectAllIdentifiers(*expr_match.match, expr_ids);
      for (const auto& id : expr_ids) {
        auto it = input_ports.find(id);
        if (it != input_ports.end()) {
          it->second.used = true;
        }
      }
    }
    
    // Report unused inputs
    for (const auto& [port_name, info] : input_ports) {
      if (!info.used) {
        std::string reason = absl::StrCat(
            "Input port '", port_name,
            "' is declared but never used (floating input). "
            "[GJB 10157 R-2-10]");
        violations_.insert(LintViolation(*info.token, reason, module_match.context));
      }
    }
  }
}

verible::LintRuleStatus Gjb10157R210Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
