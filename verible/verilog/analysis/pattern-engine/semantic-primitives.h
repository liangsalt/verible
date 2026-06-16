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

// Registry of semantic primitives callable from a YAML rule's condition.
// Each primitive is a pure(ish) function: given an EvalContext (bindings +
// project view), return bool. New primitives plug in via RegisterPrimitive
// at static init time.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SEMANTIC_PRIMITIVES_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SEMANTIC_PRIMITIVES_H_

#include <functional>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

using PrimitiveFn =
    std::function<absl::StatusOr<bool>(const PrimitiveCall &, const EvalContext &)>;

// Register a primitive. Re-registering an existing name overwrites it (last
// loaded wins). The registry is process-global -- A1 callers register all
// primitives at startup via EnsureBuiltinsRegistered().
void RegisterPrimitive(std::string_view name, PrimitiveFn fn);

// Look up a primitive by name. Returns nullptr if unknown.
const PrimitiveFn *FindPrimitive(std::string_view name);

// Idempotently register the built-in primitives: bit_width, bit_width_ge,
// bit_width_le. Other primitives (is_registered, clock_domain, ...) land
// in later A3 increments.
void EnsureBuiltinsRegistered();

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_SEMANTIC_PRIMITIVES_H_
