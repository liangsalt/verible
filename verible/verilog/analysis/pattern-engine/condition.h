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

// Structured AST for the `condition:` section of a YAML rule, plus an
// evaluator that runs it against a bindings map captured from a pattern
// match. Primitives (bit_width, is_registered, ...) are resolved through
// the semantic-primitives registry.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CONDITION_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CONDITION_H_

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/statusor.h"
#include "verible/common/text/symbol.h"

namespace verilog {

class VerilogSourceFile;
class VerilogProject;

namespace analysis {
namespace pattern_engine {

// One scalar argument to a primitive call. Integers cover bit_width_ge etc.;
// strings cover future text comparisons. Empty variant = no argument.
using PrimitiveArg = std::variant<std::monostate, int64_t, std::string>;

struct PrimitiveCall {
  std::string name;     // primitive name, e.g. "bit_width_ge"
  std::string target;   // bind id referenced by the primitive
  PrimitiveArg arg;     // optional scalar argument
};

// AST node for a condition tree: either a boolean combinator over children,
// or a leaf primitive call. Uses unique_ptr for self-recursion.
struct ConditionNode {
  enum class Op { kAll, kAny, kNot, kPrimitive };
  Op op = Op::kAll;
  std::vector<std::unique_ptr<ConditionNode>> children;  // for kAll/kAny/kNot
  PrimitiveCall primitive;                                // for kPrimitive
};

// Context passed to every primitive during evaluation. Bindings map a YAML
// `bind:` id to the captured CST symbol. project / current_file let
// primitives walk the wider context (e.g., a bit_width primitive looks back
// for the binding's declaration).
struct EvalContext {
  const std::map<std::string, const verible::Symbol *> *bindings = nullptr;
  const VerilogProject *project = nullptr;
  const VerilogSourceFile *current_file = nullptr;
};

// Evaluate a (sub)condition. Returns OK + bool, or an error if a primitive
// is unknown / misuses its argument. The empty condition (nullptr) is
// treated as "true" -- equivalent to "no condition".
absl::StatusOr<bool> EvaluateCondition(const ConditionNode *cond,
                                        const EvalContext &ctx);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CONDITION_H_
