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

#include "verible/verilog/analysis/pattern-engine/semantic-primitives.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/pattern-engine/bit-width-resolver.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

absl::flat_hash_map<std::string, PrimitiveFn> &Registry() {
  static auto *r = new absl::flat_hash_map<std::string, PrimitiveFn>;
  return *r;
}

std::mutex &RegistryMutex() {
  static std::mutex *m = new std::mutex;
  return *m;
}

// Resolve a YAML `target:` (bind id) to the captured Symbol. Returns
// nullptr if the binding is missing -- the calling primitive should fail
// permissively in that case.
const verible::Symbol *LookupBinding(const PrimitiveCall &call,
                                      const EvalContext &ctx) {
  if (ctx.bindings == nullptr) return nullptr;
  auto it = ctx.bindings->find(call.target);
  if (it == ctx.bindings->end()) return nullptr;
  return it->second;
}

absl::StatusOr<int64_t> ExpectIntArg(const PrimitiveCall &call) {
  if (const auto *v = std::get_if<int64_t>(&call.arg)) return *v;
  return absl::InvalidArgumentError(
      absl::StrCat("primitive ", call.name, " requires an integer arg"));
}

// bit_width(target) -- not a boolean primitive but useful as a building
// block. Surface it as a "true if width is known" check; for actual
// numeric thresholds use bit_width_ge/le.
absl::StatusOr<bool> PrimitiveBitWidth(const PrimitiveCall &call,
                                       const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr) return false;
  if (ctx.project == nullptr) return false;
  auto w = ResolveBitWidth(*sym, *ctx.project, ctx.current_file);
  return w.has_value();
}

absl::StatusOr<bool> PrimitiveBitWidthGE(const PrimitiveCall &call,
                                         const EvalContext &ctx) {
  auto threshold = ExpectIntArg(call);
  if (!threshold.ok()) return threshold.status();
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr) return false;
  if (ctx.project == nullptr) return false;
  auto w = ResolveBitWidth(*sym, *ctx.project, ctx.current_file);
  if (!w) return false;  // unknown width -> not >= threshold
  return *w >= *threshold;
}

// Extract first SymbolIdentifier text from a CST subtree. Shared helper.
std::optional<std::string> FirstIdentName(const verible::Symbol &sym) {
  const auto idents = verible::SearchSyntaxTree(sym, SymbolIdentifierLeaf());
  if (idents.empty() || idents.front().match == nullptr) return std::nullopt;
  const auto *l =
      dynamic_cast<const verible::SyntaxTreeLeaf *>(idents.front().match);
  if (l == nullptr) return std::nullopt;
  return std::string(l->get().text());
}

// Get first leaf text under a node (used for "is this always_ff?").
std::optional<std::string> FirstLeafText(const verible::Symbol &sym) {
  std::optional<std::string> result;
  std::function<void(const verible::Symbol &)> visit =
      [&](const verible::Symbol &s) {
        if (result) return;
        if (s.Tag().kind == verible::SymbolKind::kLeaf) {
          const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(&s);
          if (l != nullptr) result = std::string(l->get().text());
          return;
        }
        const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
        if (n == nullptr) return;
        for (const auto &c : n->children()) {
          if (c == nullptr) continue;
          visit(*c);
          if (result) return;
        }
      };
  visit(sym);
  return result;
}

// Walk every parsed file in the project, invoking `visit` with the tree root.
template <typename F>
void ForEachParsedFile(const VerilogProject &project, F &&visit) {
  for (const auto &[_, file_ptr] : project) {
    if (file_ptr == nullptr) continue;
    const verible::TextStructureView *ts = file_ptr->GetTextStructure();
    if (ts == nullptr) continue;
    const auto &tree = ts->SyntaxTree();
    if (tree == nullptr) continue;
    visit(*tree);
  }
}

// Heuristic: is this kAlwaysStatement clocked? Accepts both modern
// `always_ff` and legacy `always @(posedge/negedge ...)` styles. The
// latter is common in picorv32 and many older designs.
bool IsClockedAlways(const verible::Symbol &as_node) {
  auto first_kw = FirstLeafText(as_node);
  if (first_kw && (*first_kw == "always_ff" || *first_kw == "always_latch"))
    return true;
  if (!first_kw || *first_kw != "always") return false;
  const auto events =
      verible::SearchSyntaxTree(as_node, verilog::NodekEventControl());
  if (events.empty() || events.front().match == nullptr) return false;
  bool clocked = false;
  std::function<void(const verible::Symbol &)> visit =
      [&](const verible::Symbol &s) {
        if (clocked) return;
        if (s.Tag().kind == verible::SymbolKind::kLeaf) {
          const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(&s);
          if (l && (l->get().text() == "posedge" || l->get().text() == "negedge"))
            clocked = true;
          return;
        }
        const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
        if (!n) return;
        for (const auto &c : n->children()) {
          if (c) visit(*c);
          if (clocked) return;
        }
      };
  visit(*events.front().match);
  return clocked;
}

// is_registered(target): true if any clocked always block in the project
// writes the binding's identifier name as a non-blocking assignment LHS.
// Accepts both `always_ff` (modern) and `always @(posedge clk)` (legacy).
absl::StatusOr<bool> PrimitiveIsRegistered(const PrimitiveCall &call,
                                            const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;

  bool found = false;
  ForEachParsedFile(*ctx.project, [&](const verible::Symbol &root) {
    if (found) return;
    const auto always_stmts =
        verible::SearchSyntaxTree(root, verilog::NodekAlwaysStatement());
    for (const auto &as : always_stmts) {
      if (as.match == nullptr) continue;
      if (!IsClockedAlways(*as.match)) continue;
      const auto nbas = verible::SearchSyntaxTree(
          *as.match, verilog::NodekNonblockingAssignmentStatement());
      for (const auto &nb : nbas) {
        if (nb.match == nullptr) continue;
        const auto lpvals =
            verible::SearchSyntaxTree(*nb.match, verilog::NodekLPValue());
        if (lpvals.empty() || lpvals.front().match == nullptr) continue;
        auto lhs_name = FirstIdentName(*lpvals.front().match);
        if (lhs_name && *lhs_name == *name) {
          found = true;
          return;
        }
      }
    }
  });
  return found;
}

// fanout(target) returns the count of references; surfaced as a status-or-int
// inside fanout_gt for thresholding.
int CountFanout(const std::string &name, const VerilogProject &project) {
  int count = 0;
  ForEachParsedFile(project, [&](const verible::Symbol &root) {
    const auto idents = verible::SearchSyntaxTree(root, SymbolIdentifierLeaf());
    for (const auto &m : idents) {
      if (m.match == nullptr) continue;
      const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(m.match);
      if (l != nullptr && l->get().text() == name) ++count;
    }
  });
  return count;
}

// fanout_gt(target, n): true if reference count > n.
absl::StatusOr<bool> PrimitiveFanoutGT(const PrimitiveCall &call,
                                       const EvalContext &ctx) {
  auto threshold = ExpectIntArg(call);
  if (!threshold.ok()) return threshold.status();
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;
  return CountFanout(*name, *ctx.project) > *threshold;
}

// Extract the clock domain string for a binding by finding the always_ff
// block that assigns to it and reading its sensitivity list. Domain text
// is the literal `"<edge>_<clock_identifier>"` (e.g. "posedge_clk_a"). If
// not driven by an always_ff, returns empty string.
std::string ClockDomainFor(const std::string &name,
                            const VerilogProject &project) {
  std::string domain;
  ForEachParsedFile(project, [&](const verible::Symbol &root) {
    if (!domain.empty()) return;
    const auto always_stmts =
        verible::SearchSyntaxTree(root, verilog::NodekAlwaysStatement());
    for (const auto &as : always_stmts) {
      if (as.match == nullptr) continue;
      if (!IsClockedAlways(*as.match)) continue;

      // Confirm the block writes `name`.
      const auto nbas = verible::SearchSyntaxTree(
          *as.match, verilog::NodekNonblockingAssignmentStatement());
      bool writes_name = false;
      for (const auto &nb : nbas) {
        if (nb.match == nullptr) continue;
        const auto lpvals =
            verible::SearchSyntaxTree(*nb.match, verilog::NodekLPValue());
        if (lpvals.empty() || lpvals.front().match == nullptr) continue;
        auto lhs_name = FirstIdentName(*lpvals.front().match);
        if (lhs_name && *lhs_name == name) {
          writes_name = true;
          break;
        }
      }
      if (!writes_name) continue;

      // Pull edge + clock id from the event control. We do a flat leaf walk
      // and pick up the first "posedge"/"negedge" followed by the next
      // SymbolIdentifier-text leaf.
      const auto events =
          verible::SearchSyntaxTree(*as.match, verilog::NodekEventControl());
      if (events.empty() || events.front().match == nullptr) continue;

      std::string edge;
      std::string clk;
      std::function<void(const verible::Symbol &)> visit =
          [&](const verible::Symbol &s) {
            if (!clk.empty()) return;
            if (s.Tag().kind == verible::SymbolKind::kLeaf) {
              const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(&s);
              if (l == nullptr) return;
              const std::string_view txt = l->get().text();
              if (txt == "posedge" || txt == "negedge") {
                edge = std::string(txt);
              } else if (!edge.empty() &&
                         !txt.empty() && (std::isalpha(static_cast<unsigned char>(txt.front())) ||
                                          txt.front() == '_')) {
                clk = std::string(txt);
              }
              return;
            }
            const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
            if (n == nullptr) return;
            for (const auto &c : n->children()) {
              if (c == nullptr) continue;
              visit(*c);
              if (!clk.empty()) return;
            }
          };
      visit(*events.front().match);
      if (!clk.empty()) {
        domain = edge + "_" + clk;
        return;
      }
    }
  });
  return domain;
}

// Count operator leaves inside a CST subtree. Verible flattens chains of
// the same operator (`a+b+c+d` becomes ONE kBinaryExpression with 4
// operand children + 3 `+` leaves), so a nested-node count would
// under-report the actual chain length. Counting operator leaves matches
// the "logic levels" that Vivado / synthesis would build.
static bool IsCombOperator(std::string_view t) {
  static const char *const kOps[] = {
      "+", "-", "*", "/", "%", "&", "|", "^",
      "==", "!=", "<", ">", "<=", ">=", "<<", ">>",
      "&&", "||", "~", "!",
  };
  for (const char *op : kOps) if (t == op) return true;
  return false;
}

int CombDepth(const verible::Symbol &root) {
  int count = 0;
  std::function<void(const verible::Symbol &)> visit =
      [&](const verible::Symbol &s) {
    if (s.Tag().kind == verible::SymbolKind::kLeaf) {
      const auto *l = dynamic_cast<const verible::SyntaxTreeLeaf *>(&s);
      if (l != nullptr && IsCombOperator(l->get().text())) ++count;
      return;
    }
    const auto *n = dynamic_cast<const verible::SyntaxTreeNode *>(&s);
    if (n == nullptr) return;
    for (const auto &c : n->children()) {
      if (c == nullptr) continue;
      visit(*c);
    }
  };
  visit(root);
  return count;
}

// is_fsm_state_register(target): true if the binding's identifier is
// (a) the LHS of a non-blocking assignment inside an always_ff block, AND
// (b) the expression a kCaseStatement inside that same always_ff branches
// on. This catches the canonical FSM coding style:
//
//   always_ff @(posedge clk) begin
//     case (state)               <-- name appears here
//       S0: state <= S1;         <-- and here as LHS
//       S1: state <= S2;
//       ...
//     endcase
//   end
//
// Multi-clock FSM (one always_ff for state, another for next-state) is
// not detected -- the two halves are linked by a separate combinational
// block. We accept the false negative for the MVP.
bool IsFsmStateRegister(const std::string &name,
                       const VerilogProject &project) {
  bool found = false;
  ForEachParsedFile(project, [&](const verible::Symbol &root) {
    if (found) return;
    const auto always_stmts =
        verible::SearchSyntaxTree(root, verilog::NodekAlwaysStatement());
    for (const auto &as : always_stmts) {
      if (as.match == nullptr) continue;
      if (!IsClockedAlways(*as.match)) continue;

      // (a) name is a non-blocking LHS inside this block.
      bool name_is_nb_lhs = false;
      const auto nbas = verible::SearchSyntaxTree(
          *as.match, verilog::NodekNonblockingAssignmentStatement());
      for (const auto &nb : nbas) {
        if (nb.match == nullptr) continue;
        const auto lpvals =
            verible::SearchSyntaxTree(*nb.match, verilog::NodekLPValue());
        if (lpvals.empty() || lpvals.front().match == nullptr) continue;
        auto lhs_name = FirstIdentName(*lpvals.front().match);
        if (lhs_name && *lhs_name == name) {
          name_is_nb_lhs = true;
          break;
        }
      }
      if (!name_is_nb_lhs) continue;

      // (b) name appears as a case expression inside this block. We look
      // for kCaseStatement, then walk its first kExpression / kReference
      // child to find the identifier being branched on.
      const auto cases =
          verible::SearchSyntaxTree(*as.match, verilog::NodekCaseStatement());
      for (const auto &cs : cases) {
        if (cs.match == nullptr) continue;
        // The case expression is the kExpression child immediately after
        // the "case" keyword. Search for any kReference inside the case
        // node up to (but not including) any kCaseItem -- this is good
        // enough as a heuristic.
        const auto refs =
            verible::SearchSyntaxTree(*cs.match, verilog::NodekReference());
        if (refs.empty() || refs.front().match == nullptr) continue;
        auto ref_name = FirstIdentName(*refs.front().match);
        if (ref_name && *ref_name == name) {
          found = true;
          return;
        }
      }
    }
  });
  return found;
}

absl::StatusOr<bool> PrimitiveIsFsmStateRegister(const PrimitiveCall &call,
                                                 const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;
  return IsFsmStateRegister(*name, *ctx.project);
}

// comb_depth_gt(target, arg): true if the binding's subtree has more than
// `arg` nested binary/unary expressions. Catches deep arithmetic /
// comparator chains that tend to dominate critical paths.
absl::StatusOr<bool> PrimitiveCombDepthGT(const PrimitiveCall &call,
                                          const EvalContext &ctx) {
  auto threshold = ExpectIntArg(call);
  if (!threshold.ok()) return threshold.status();
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr) return false;
  return CombDepth(*sym) > *threshold;
}

// clock_domain(target): returns true iff the binding has a resolvable
// clock-domain string.
absl::StatusOr<bool> PrimitiveClockDomain(const PrimitiveCall &call,
                                          const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;
  return !ClockDomainFor(*name, *ctx.project).empty();
}

// cross_clock_domain_no_synchronizer(target, arg=<other_bind>): true if
// the binding's signal and the signal captured under bind name `arg`
// resolve to DIFFERENT clock domains. Flags direct register-to-register
// handoffs across clocks without a 2-FF synchronizer in between.
// The `arg` is a STRING naming another bind in the same pattern.
absl::StatusOr<bool> PrimitiveCrossClockDomain(const PrimitiveCall &call,
                                                const EvalContext &ctx) {
  const auto *str_arg = std::get_if<std::string>(&call.arg);
  if (str_arg == nullptr) {
    return absl::InvalidArgumentError(
        "cross_clock_domain_no_synchronizer requires string `arg` "
        "(name of another bind)");
  }
  if (ctx.bindings == nullptr || ctx.project == nullptr) return false;

  const auto *sym_a = LookupBinding(call, ctx);
  auto it_b = ctx.bindings->find(*str_arg);
  if (sym_a == nullptr || it_b == ctx.bindings->end() || it_b->second == nullptr)
    return false;
  auto name_a = FirstIdentName(*sym_a);
  auto name_b = FirstIdentName(*it_b->second);
  if (!name_a || !name_b) return false;

  const std::string dom_a = ClockDomainFor(*name_a, *ctx.project);
  const std::string dom_b = ClockDomainFor(*name_b, *ctx.project);
  // Both must be resolvable; otherwise no confident call.
  if (dom_a.empty() || dom_b.empty()) return false;
  return dom_a != dom_b;
}

// drives_comb_logic_to_register(target): true if the binding's identifier
// appears on the RHS of a non-blocking assignment inside an always_ff
// block, but is NOT the LHS. Captures the canonical "wide signal feeds
// into another register through comb" critical-path pattern.
absl::StatusOr<bool> PrimitiveDrivesCombToReg(const PrimitiveCall &call,
                                               const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;

  bool found = false;
  ForEachParsedFile(*ctx.project, [&](const verible::Symbol &root) {
    if (found) return;
    const auto always_stmts =
        verible::SearchSyntaxTree(root, verilog::NodekAlwaysStatement());
    for (const auto &as : always_stmts) {
      if (as.match == nullptr) continue;
      if (!IsClockedAlways(*as.match)) continue;

      const auto nbas = verible::SearchSyntaxTree(
          *as.match, verilog::NodekNonblockingAssignmentStatement());
      for (const auto &nb : nbas) {
        if (nb.match == nullptr) continue;
        const auto lpvals =
            verible::SearchSyntaxTree(*nb.match, verilog::NodekLPValue());
        if (lpvals.empty() || lpvals.front().match == nullptr) continue;
        auto lhs_name = FirstIdentName(*lpvals.front().match);
        // The signal must NOT be the LHS here.
        if (lhs_name && *lhs_name == *name) continue;

        const auto idents =
            verible::SearchSyntaxTree(*nb.match, SymbolIdentifierLeaf());
        for (const auto &i : idents) {
          if (i.match == nullptr) continue;
          const auto *l =
              dynamic_cast<const verible::SyntaxTreeLeaf *>(i.match);
          if (l != nullptr && l->get().text() == *name) {
            found = true;
            return;
          }
        }
      }
    }
  });
  return found;
}

// fanout(target): used as a "has any references at all" boolean check.
absl::StatusOr<bool> PrimitiveFanout(const PrimitiveCall &call,
                                     const EvalContext &ctx) {
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr || ctx.project == nullptr) return false;
  auto name = FirstIdentName(*sym);
  if (!name) return false;
  return CountFanout(*name, *ctx.project) > 0;
}

absl::StatusOr<bool> PrimitiveBitWidthLE(const PrimitiveCall &call,
                                         const EvalContext &ctx) {
  auto threshold = ExpectIntArg(call);
  if (!threshold.ok()) return threshold.status();
  const auto *sym = LookupBinding(call, ctx);
  if (sym == nullptr) return false;
  if (ctx.project == nullptr) return false;
  auto w = ResolveBitWidth(*sym, *ctx.project, ctx.current_file);
  if (!w) return false;
  return *w <= *threshold;
}

std::atomic<bool> g_builtins_registered{false};
std::mutex g_builtins_mutex;

}  // namespace

void RegisterPrimitive(std::string_view name, PrimitiveFn fn) {
  std::lock_guard<std::mutex> lock(RegistryMutex());
  Registry()[std::string(name)] = std::move(fn);
}

const PrimitiveFn *FindPrimitive(std::string_view name) {
  std::lock_guard<std::mutex> lock(RegistryMutex());
  auto &r = Registry();
  auto it = r.find(std::string(name));
  if (it == r.end()) return nullptr;
  return &it->second;
}

void EnsureBuiltinsRegistered() {
  if (g_builtins_registered.load(std::memory_order_acquire)) return;
  std::lock_guard<std::mutex> lock(g_builtins_mutex);
  if (g_builtins_registered.load(std::memory_order_acquire)) return;
  RegisterPrimitive("bit_width", PrimitiveBitWidth);
  RegisterPrimitive("bit_width_ge", PrimitiveBitWidthGE);
  RegisterPrimitive("bit_width_le", PrimitiveBitWidthLE);
  RegisterPrimitive("is_registered", PrimitiveIsRegistered);
  RegisterPrimitive("clock_domain", PrimitiveClockDomain);
  RegisterPrimitive("cross_clock_domain_no_synchronizer",
                    PrimitiveCrossClockDomain);
  RegisterPrimitive("drives_comb_logic_to_register",
                    PrimitiveDrivesCombToReg);
  RegisterPrimitive("comb_depth_gt", PrimitiveCombDepthGT);
  RegisterPrimitive("is_fsm_state_register", PrimitiveIsFsmStateRegister);
  RegisterPrimitive("fanout", PrimitiveFanout);
  RegisterPrimitive("fanout_gt", PrimitiveFanoutGT);
  g_builtins_registered.store(true, std::memory_order_release);
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
