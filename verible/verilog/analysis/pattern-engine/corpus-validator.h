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

// Enforces the corpus contract on a rule: examples.bad MUST hit, examples.good
// MUST NOT hit. This is the hard gate before a rule is allowed to be active.
// Both the AI authoring loop and bazel test invoke this to validate each rule
// before it joins the RuleSet seen by /rtl-scan.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CORPUS_VALIDATOR_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CORPUS_VALIDATOR_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

struct CorpusResult {
  bool bad_hit = false;
  bool good_hit = false;
  std::vector<std::string> bad_diagnostics;
  std::vector<std::string> good_diagnostics;
  bool passed() const { return bad_hit && !good_hit; }
};

// Compile the rule's pattern + condition, run it against examples.bad and
// examples.good, and return what hit where. A passing result means
// (bad_hit && !good_hit).
//
// Implementation lands in A1.7.
absl::StatusOr<CorpusResult> ValidateCorpus(const LoadedRule& rule);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_CORPUS_VALIDATOR_H_
