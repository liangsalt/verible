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

// Minimum-viable bit-width resolver for the A3 pattern engine.
//
// Resolves the declared width of a CST symbol when that symbol is (or
// contains) an identifier reference whose declaration is in scope. Handles
// the declaration form `wire [N:M] foo;` and friends with literal integer
// range bounds. Symbolic expressions (parameters, $clog2, etc.) are
// reported as "unknown" rather than guessed.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_BIT_WIDTH_RESOLVER_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_BIT_WIDTH_RESOLVER_H_

#include <optional>

#include "verible/common/text/symbol.h"

namespace verilog {

class VerilogProject;
class VerilogSourceFile;

namespace analysis {
namespace pattern_engine {

// Try to compute the bit-width of `target` by:
//   1. extracting its SymbolIdentifier text (if `target` is/contains one),
//   2. scanning the project's parsed files for a declaration of that name,
//   3. parsing its [N:M] range as literal integers.
//
// Returns nullopt when the width can't be determined (no identifier,
// declaration not found, symbolic range, etc.). The caller -- typically
// the bit_width_ge primitive -- treats nullopt as "primitive does not
// apply" rather than "primitive is false" so authors can keep their rules
// permissive about unresolvable signals.
std::optional<int> ResolveBitWidth(const verible::Symbol &target,
                                    const VerilogProject &project,
                                    const VerilogSourceFile *origin_file);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_BIT_WIDTH_RESOLVER_H_
