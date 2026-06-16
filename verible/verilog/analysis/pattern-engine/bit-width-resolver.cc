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

#include "verible/verilog/analysis/pattern-engine/bit-width-resolver.h"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

// Extract the first SymbolIdentifier leaf's text from a symbol subtree.
std::optional<std::string> FirstIdentifierText(const verible::Symbol &sym) {
  const auto idents = verible::SearchSyntaxTree(sym, SymbolIdentifierLeaf());
  if (idents.empty() || idents.front().match == nullptr) return std::nullopt;
  const auto *leaf =
      dynamic_cast<const verible::SyntaxTreeLeaf *>(idents.front().match);
  if (leaf == nullptr) return std::nullopt;
  return std::string(leaf->get().text());
}

// Parse a TK_DecNumber leaf's text as int. Returns nullopt on overflow / junk.
std::optional<int64_t> ParseDecLeaf(const verible::SyntaxTreeLeaf &leaf) {
  const std::string_view t = leaf.get().text();
  // Allow a leading sign for tolerance, though Verilog ranges don't use them.
  char *end = nullptr;
  const std::string s(t);
  errno = 0;
  const int64_t v = std::strtoll(s.c_str(), &end, 10);
  if (end == s.c_str() || errno != 0) return std::nullopt;
  return v;
}

// Inside a kDimensionRange node, find the two kNumber leaves bracketing ':'
// and return high - low + 1 (1-based inclusive width). Returns nullopt if
// the range uses symbolic expressions.
std::optional<int> WidthFromRange(const verible::SyntaxTreeNode &range_node) {
  std::optional<int64_t> first;
  std::optional<int64_t> second;
  for (const auto &child : range_node.children()) {
    if (child == nullptr) continue;
    if (child->Tag().kind != verible::SymbolKind::kNode) continue;
    if (static_cast<verilog::NodeEnum>(child->Tag().tag) !=
        verilog::NodeEnum::kExpression) {
      continue;
    }
    // Within each kExpression, look for a kNumber > leaf TK_DecNumber.
    const auto *expr_node =
        dynamic_cast<const verible::SyntaxTreeNode *>(child.get());
    if (expr_node == nullptr) continue;
    // Walk for first leaf whose text parses as int. Cheap & robust.
    std::optional<int64_t> parsed;
    for (const auto &gchild : expr_node->children()) {
      if (gchild == nullptr) continue;
      const auto leaves =
          verible::SearchSyntaxTree(*gchild, SymbolIdentifierLeaf());
      // We don't want identifiers; we want number leaves. Walk all leaves
      // and pick the first one whose text is a pure integer.
      // Easier: traverse all leaves manually.
      // Use a recursive helper via lambda.
      std::function<void(const verible::Symbol &)> visit =
          [&](const verible::Symbol &s) {
            if (parsed) return;
            if (s.Tag().kind == verible::SymbolKind::kLeaf) {
              const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(&s);
              if (l != nullptr) {
                auto v = ParseDecLeaf(*l);
                if (v) parsed = v;
              }
              return;
            }
            const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
            if (n == nullptr) return;
            for (const auto &c : n->children()) {
              if (c == nullptr) continue;
              visit(*c);
              if (parsed) return;
            }
          };
      visit(*gchild);
      if (parsed) break;
    }
    if (!first) first = parsed;
    else if (!second) {
      second = parsed;
      break;
    }
  }
  if (!first || !second) return std::nullopt;
  const int64_t hi = std::max(*first, *second);
  const int64_t lo = std::min(*first, *second);
  return static_cast<int>(hi - lo + 1);
}

// Walk into a declaration node and return its packed-dimensions width.
// 1 if there are no packed dimensions (scalar), nullopt if dimensions are
// symbolic, otherwise the literal range width.
std::optional<int> WidthFromDeclaration(const verible::SyntaxTreeNode &decl) {
  // Search for a kDimensionRange descendant. There's exactly one in well-
  // formed packed dimensions; if there are multiple (e.g. `reg [3:0][7:0]`),
  // we just take the first -- this is "outer" packed width.
  using verilog::NodeEnum;
  std::function<const verible::SyntaxTreeNode *(const verible::Symbol &)> find =
      [&](const verible::Symbol &s) -> const verible::SyntaxTreeNode * {
    if (s.Tag().kind != verible::SymbolKind::kNode) return nullptr;
    const auto *node = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
    if (node == nullptr) return nullptr;
    if (static_cast<NodeEnum>(node->Tag().tag) == NodeEnum::kDimensionRange) {
      return node;
    }
    for (const auto &c : node->children()) {
      if (c == nullptr) continue;
      const auto *hit = find(*c);
      if (hit != nullptr) return hit;
    }
    return nullptr;
  };
  const auto *range = find(decl);
  if (range == nullptr) return 1;  // no packed dimensions -> 1-bit
  return WidthFromRange(*range);
}

// Test whether a declaration node declares `name`.
bool DeclarationDeclares(const verible::SyntaxTreeNode &decl,
                        std::string_view name) {
  const auto idents = verible::SearchSyntaxTree(decl, SymbolIdentifierLeaf());
  for (const auto &m : idents) {
    if (m.match == nullptr) continue;
    const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(m.match);
    if (l != nullptr && l->get().text() == name) return true;
  }
  return false;
}

// Search the parsed CST of `tree` for a declaration of `name`. Returns
// pointer to the declaration node, or nullptr.
const verible::SyntaxTreeNode *FindDeclarationInTree(const verible::Symbol &tree,
                                                     std::string_view name) {
  // Try kDataDeclaration first (covers reg / logic), then kNetDeclaration
  // (covers wire), then kPortDeclaration (covers input/output decls).
  // Each scan is its own SearchSyntaxTree pass.
  auto search = [&](const auto &matcher) -> const verible::SyntaxTreeNode * {
    const auto hits = verible::SearchSyntaxTree(tree, matcher());
    for (const auto &h : hits) {
      if (h.match == nullptr) continue;
      const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(h.match);
      if (n == nullptr) continue;
      if (DeclarationDeclares(*n, name)) return n;
    }
    return nullptr;
  };
  if (const auto *n = search(verilog::NodekDataDeclaration); n != nullptr)
    return n;
  if (const auto *n = search(verilog::NodekNetDeclaration); n != nullptr)
    return n;
  if (const auto *n = search(verilog::NodekPortDeclaration); n != nullptr)
    return n;
  return nullptr;
}

}  // namespace

std::optional<int> ResolveBitWidth(const verible::Symbol &target,
                                    const VerilogProject &project,
                                    const VerilogSourceFile *origin_file) {
  const auto name = FirstIdentifierText(target);
  if (!name) return std::nullopt;

  // Look in origin_file first (typical hit).
  auto try_file = [&](const VerilogSourceFile *f) -> std::optional<int> {
    if (f == nullptr) return std::nullopt;
    const verible::TextStructureView *ts = f->GetTextStructure();
    if (ts == nullptr) return std::nullopt;
    const auto &tree = ts->SyntaxTree();
    if (tree == nullptr) return std::nullopt;
    const auto *decl = FindDeclarationInTree(*tree, *name);
    if (decl == nullptr) return std::nullopt;
    return WidthFromDeclaration(*decl);
  };
  if (auto w = try_file(origin_file); w) return w;

  // Fall back to scanning the whole project.
  for (const auto &[_, file_ptr] : project) {
    if (file_ptr.get() == origin_file) continue;
    if (file_ptr == nullptr) continue;
    if (auto w = try_file(file_ptr.get()); w) return w;
  }
  return std::nullopt;
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
