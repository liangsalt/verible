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

#include "verible/verilog/analysis/checkers/v2001-procedural-decls-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/statement.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(V2001ProceduralDeclsRule);

const LintRuleDescriptor &V2001ProceduralDeclsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "v2001-procedural-decls",
      .topic = "procedural-declarations",
      .desc =
          "In Verilog-2001 (.v) files, procedural blocks (initial/always/final) "
          "must not contain declarations. Variables should be declared at "
          "module scope before the block. Typed for-loop initializations "
          "(e.g. `for(integer i=0; ...)`) are also disallowed.",
  };
  return d;
}

absl::Status V2001ProceduralDeclsRule::Configure(
    std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

namespace {

bool IsDeclarationNode(const verible::Symbol &symbol) {
  if (symbol.Kind() != verible::SymbolKind::kNode) return false;
  switch (NodeEnum(symbol.Tag().tag)) {
    case NodeEnum::kDataDeclaration:
    case NodeEnum::kNetTypeDeclaration:
    case NodeEnum::kTypeDeclaration:
    case NodeEnum::kLetDeclaration:
    case NodeEnum::kPackageImportDeclaration:
    case NodeEnum::kParamDeclaration:
      return true;
    default:
      return false;
  }
}

// Returns the block body (statement) of a procedural construct if present.
const SyntaxTreeNode *GetProceduralBody(const verible::Symbol &procedural) {
  const auto &node = verible::SymbolCastToNode(procedural);
  switch (NodeEnum(node.Tag().tag)) {
    case NodeEnum::kInitialStatement:
      return verible::GetSubtreeAsNode(node, NodeEnum::kInitialStatement, 1);
    case NodeEnum::kAlwaysStatement:
      return verible::GetSubtreeAsNode(node, NodeEnum::kAlwaysStatement, 1);
    case NodeEnum::kFinalStatement:
      return verible::GetSubtreeAsNode(node, NodeEnum::kFinalStatement, 1);
    default:
      return nullptr;
  }
}

// Collect typed for-inits within a seq block.
std::vector<const verible::Symbol *> FindTypedForInitializations(
    const SyntaxTreeNode &seq_block) {
  std::vector<const verible::Symbol *> typed_inits;
  for (const auto &match : FindAllForLoopsInitializations(seq_block)) {
    const auto *data_type = GetDataTypeFromForInitialization(*match.match);
    if (data_type != nullptr) typed_inits.push_back(match.match);
  }
  return typed_inits;
}

}  // namespace

void V2001ProceduralDeclsRule::Lint(
    const verible::TextStructureView &text_structure,
    std::string_view filename) {
  if (!absl::EndsWith(filename, ".v")) return;

  const auto &base = text_structure.Contents();
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  const auto &line_map = text_structure.GetLineColumnMap();
  auto line_of = [&](const verible::TokenInfo &token) {
    return line_map.GetLineColAtOffset(base, token.left(base)).line + 1;
  };

  auto analyze_procedural = [&](const verible::Symbol &procedural_symbol,
                                const verible::SyntaxTreeContext &context) {
    const auto *body = GetProceduralBody(procedural_symbol);
    if (body == nullptr) return;

    const auto *start_leaf = verible::GetLeftmostLeaf(procedural_symbol);
    const int block_line =
        start_leaf ? line_of(start_leaf->get()) : 0;  // fallback 0

    const SyntaxTreeNode *seq_block = nullptr;
    if (body->Tag().tag == static_cast<int>(NodeEnum::kSeqBlock)) {
      seq_block = body;
    } else {
      return;  // Only handle begin/end blocks
    }

    const auto *item_list =
        verible::GetSubtreeAsNode(*seq_block, NodeEnum::kSeqBlock, 1);
    if (item_list != nullptr) {
      for (const auto &child : item_list->children()) {
        const auto *symbol = child.get();
        if (symbol == nullptr) continue;
        if (!IsDeclarationNode(*symbol)) continue;
        const auto *leftmost = verible::GetLeftmostLeaf(*symbol);
        if (leftmost == nullptr) continue;
        const int decl_line = line_of(leftmost->get());
        const std::string reason =
            absl::StrCat("line ", decl_line, ": declaration '",
                         leftmost->get().text(),
                         "' is not allowed inside this procedural block for "
                         "Verilog-2001 (.v). Move it before the block (around "
                         "line ",
                         block_line, ") at module scope, then use it inside.");
        violations_.insert(LintViolation(leftmost->get(), reason, context));
      }
    }

    for (const auto *typed_init : FindTypedForInitializations(*seq_block)) {
      const auto *data_type = GetDataTypeFromForInitialization(*typed_init);
      const auto *type_leaf =
          data_type ? verible::GetLeftmostLeaf(*data_type) : nullptr;
      if (type_leaf == nullptr) continue;
      const int for_line = line_of(type_leaf->get());
      const std::string reason =
          absl::StrCat("line ", for_line,
                       ": typed for-loop initializer '",
                       type_leaf->get().text(),
                       "' is not allowed in Verilog-2001 (.v). Declare the "
                       "variable before the block (around line ",
                       block_line,
                       "), then write the loop as 'for (i = ... )' inside.");
      violations_.insert(LintViolation(type_leaf->get(), reason, context));
    }
  };

  for (const auto &match : SearchSyntaxTree(*tree, NodekInitialStatement())) {
    analyze_procedural(*match.match, match.context);
  }
  for (const auto &match : SearchSyntaxTree(*tree, NodekAlwaysStatement())) {
    analyze_procedural(*match.match, match.context);
  }
  for (const auto &match : SearchSyntaxTree(*tree, NodekFinalStatement())) {
    analyze_procedural(*match.match, match.context);
  }
}

verible::LintRuleStatus V2001ProceduralDeclsRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
