// Copyright 2017-2020 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_DEFAULT_RULES_H_
#define VERIBLE_VERILOG_ANALYSIS_DEFAULT_RULES_H_

namespace verilog {
namespace analysis {

// List of rule that are enabled by default in production
// Use --ruleset default
// clang-format off
// LINT.IfChange
inline constexpr const char* kDefaultRuleSet[] = {
    "invalid-system-task-function",
    "module-begin-block",
    "module-parameter",
    "module-port",
    "module-filename",
    "package-filename",
    "void-cast",
    "forbid-line-continuations",
    "generate-label",
    "generate-label-prefix",
    "GJB-10157-R-2-1",
    "GJB-10157-R-2-2",
    "GJB-10157-R-2-3",
    "GJB-10157-R-2-4",
    "GJB-10157-R-2-5",
    "GJB-10157-R-2-6",
    "GJB-10157-R-2-7",
    "GJB-10157-R-2-8",
    "GJB-10157-R-2-9",
    "GJB-10157-R-2-10",
    "GJB-10157-A-2-1",
    "always-comb",
    //"disable-statement", // temporary disabled
    "v2001-generate-begin",
    "forbidden-macro",
    "create-object-name-match",
    "packed-dimensions-range-ordering",
    "unpacked-dimensions-range-ordering",
    "no-trailing-spaces",
    "no-tabs",
    "posix-eof",
    "line-length",
    "undersized-binary-literal",
    "explicit-function-lifetime",
    "explicit-function-task-parameter-type",
    "explicit-task-lifetime",
    // "explicit-parameter-storage-type",  // disabled by default
    "plusarg-assignment",
    "macro-name-style",
    // "parameter-name-style",  // disabled by default
    "typedef-enums",
    "forbid-defparam",
    "typedef-structs-unions",
    "always-comb-blocking",
    "always-ff-non-blocking",
    "forbid-consecutive-null-statements",
    "enum-name-style",
    "struct-union-name-style",
    "case-missing-default",
    "interface-name-style",
    "positive-meaning-parameter-name",
    "constraint-name-style",
    "suggest-parentheses",
    "truncated-numeric-literal",
    "v2001-procedural-decls",
    // TODO(fangism): enable in production:
    // TODO(b/155128436): "uvm-macro-semicolon"
    // TODO(b/117131903): "proper-parameter-declaration",
    // TODO(b/131637160): "signal-name-style",
    // TODO(b/120784977): "numeric-format-string-style",
    // TODO: "one-module-per-file",
    // TODO: "banned-declared-name-patterns",
    // TODO: "port-name-suffix",
    // "endif-comment",
    // "instance-shadowing",
};
// LINT.ThenChange(../tools/lint/BUILD)
//   Update integration tests for rulesets.
// clang-format on

// GJB 10157 ruleset - includes all GJB rules (pure Verilog compatible)
// Use --ruleset gjb
// clang-format off
inline constexpr const char* kGJBRuleSet[] = {
    // GJB 10157 Required rules (R-2-x) - error severity
    "GJB-10157-R-2-1",
    "GJB-10157-R-2-2",
    "GJB-10157-R-2-3",
    "GJB-10157-R-2-4",
    "GJB-10157-R-2-5",
    "GJB-10157-R-2-6",
    "GJB-10157-R-2-7",
    "GJB-10157-R-2-8",
    "GJB-10157-R-2-9",
    "GJB-10157-R-2-10",
    // GJB 10157 Advisory rules (A-2-x) - warning severity
    "GJB-10157-A-2-1",
    // Note: always-comb and always-comb-blocking are NOT included
    // because they are SystemVerilog-only rules (always_comb keyword)
    // Note: explicit-parameter-storage-type, parameter-name-style,
    // and instance-shadowing are intentionally NOT included (disabled)
};
// clang-format on

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_DEFAULT_RULES_H_
