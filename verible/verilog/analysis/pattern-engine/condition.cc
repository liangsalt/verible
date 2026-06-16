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

#include "verible/verilog/analysis/pattern-engine/condition.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "verible/verilog/analysis/pattern-engine/semantic-primitives.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

absl::StatusOr<bool> EvaluateCondition(const ConditionNode *cond,
                                        const EvalContext &ctx) {
  if (cond == nullptr) return true;  // no condition = pass-through

  switch (cond->op) {
    case ConditionNode::Op::kAll:
      for (const auto &c : cond->children) {
        auto sub = EvaluateCondition(c.get(), ctx);
        if (!sub.ok()) return sub.status();
        if (!*sub) return false;
      }
      return true;
    case ConditionNode::Op::kAny:
      for (const auto &c : cond->children) {
        auto sub = EvaluateCondition(c.get(), ctx);
        if (!sub.ok()) return sub.status();
        if (*sub) return true;
      }
      return false;
    case ConditionNode::Op::kNot: {
      if (cond->children.size() != 1) {
        return absl::InvalidArgumentError(
            "condition 'not' requires exactly one child");
      }
      auto sub = EvaluateCondition(cond->children[0].get(), ctx);
      if (!sub.ok()) return sub.status();
      return !*sub;
    }
    case ConditionNode::Op::kPrimitive: {
      const PrimitiveFn *fn = FindPrimitive(cond->primitive.name);
      if (fn == nullptr) {
        return absl::NotFoundError(
            absl::StrCat("unknown primitive: ", cond->primitive.name));
      }
      return (*fn)(cond->primitive, ctx);
    }
  }
  return absl::InternalError("unreachable condition op");
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
