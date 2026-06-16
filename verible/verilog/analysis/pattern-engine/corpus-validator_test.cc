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

// A1.9: loads every YAML rule in the repository's rules/ directory and
// asserts that its bad.sv corpus example hits and its good.sv example
// does not. This is the hard gate keeping a rule in the production
// RuleSet -- corpus-validator failures are blocking.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "verible/verilog/analysis/pattern-engine/corpus-validator.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {
namespace {

// On Windows, Bazel ships test data via a runfiles MANIFEST instead of a
// physical directory tree. Parse it and return a map from logical path
// (e.g. "_main/rules/foo.yaml") to absolute source path on disk.
std::map<std::string, std::string> LoadRunfilesManifest() {
  std::map<std::string, std::string> result;
  const char* mf = std::getenv("RUNFILES_MANIFEST_FILE");
  if (mf == nullptr) return result;
  std::ifstream in(mf);
  if (!in) return result;
  std::string line;
  while (std::getline(in, line)) {
    // Format: "<logical> <source>" (space separated)
    auto sp = line.find(' ');
    if (sp == std::string::npos) continue;
    result[line.substr(0, sp)] = line.substr(sp + 1);
  }
  return result;
}

// Resolve the on-disk path of a YAML rule file given its logical runfiles
// path. Falls back to the logical path itself (useful on Linux where the
// runfiles tree exists physically).
std::string Resolve(const std::map<std::string, std::string>& manifest,
                    const std::string& logical) {
  if (auto it = manifest.find(logical); it != manifest.end()) {
    return it->second;
  }
  return logical;
}

class SeedRulesCorpusTest : public ::testing::Test {};

TEST_F(SeedRulesCorpusTest, EverySeedRulePassesCorpus) {
  const auto manifest = LoadRunfilesManifest();

  // Collect all logical paths under _main/rules/ that end in .yaml.
  std::vector<std::string> yaml_logical;
  for (const auto& [logical, src] : manifest) {
    constexpr std::string_view kPrefix = "_main/rules/";
    if (logical.rfind(kPrefix, 0) != 0) continue;
    if (logical.size() < 5) continue;
    if (logical.substr(logical.size() - 5) != ".yaml") continue;
    // Skip files in subdirectories (we only want top-level rules).
    if (logical.find('/', kPrefix.size()) != std::string::npos) continue;
    yaml_logical.push_back(logical);
  }

  // Fallback: if the manifest was empty (Linux non-manifest case), walk the
  // physical rules/ dir relative to cwd.
  std::vector<std::string> yaml_paths;
  if (!yaml_logical.empty()) {
    for (const auto& logical : yaml_logical) {
      yaml_paths.push_back(Resolve(manifest, logical));
    }
  } else {
    const std::string dir = "rules";
    if (std::filesystem::exists(dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
          yaml_paths.push_back(entry.path().string());
        }
      }
    }
  }

  ASSERT_GE(yaml_paths.size(), 5u)
      << "expected >= 5 seed rules; found " << yaml_paths.size()
      << " (manifest_size=" << manifest.size() << ")";

  for (const auto& path : yaml_paths) {
    SCOPED_TRACE(path);
    auto rule = LoadRuleFromYaml(path, RuleScope::kProject);
    ASSERT_TRUE(rule.ok())
        << "failed to load " << path << ": " << rule.status();

    auto result = ValidateCorpus(*rule);
    ASSERT_TRUE(result.ok())
        << "corpus validation crashed for " << path << ": "
        << result.status();
    EXPECT_TRUE(result->passed())
        << "rule " << rule->name << " failed corpus gate. "
        << "bad_hit=" << result->bad_hit
        << " good_hit=" << result->good_hit << " | "
        << "bad diagnostics: "
        << (result->bad_diagnostics.empty() ? "(none)"
                                            : result->bad_diagnostics.front())
        << " | good diagnostics: "
        << (result->good_diagnostics.empty() ? "(none)"
                                             : result->good_diagnostics.front());
  }
}

}  // namespace
}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
