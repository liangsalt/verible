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

#include "verible/verilog/analysis/pattern-engine/matcher-builder.h"

#include "gtest/gtest.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {
namespace {

TEST(MatcherBuilder, LookupKnownNodeWithKPrefix) {
  auto v = LookupNodeEnum("kModuleDeclaration");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, static_cast<int>(verilog::NodeEnum::kModuleDeclaration));
}

TEST(MatcherBuilder, LookupKnownNodeWithoutKPrefix) {
  auto v = LookupNodeEnum("ModuleDeclaration");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, static_cast<int>(verilog::NodeEnum::kModuleDeclaration));
}

TEST(MatcherBuilder, LookupBinaryExpression) {
  auto with_k = LookupNodeEnum("kBinaryExpression");
  auto without_k = LookupNodeEnum("BinaryExpression");
  ASSERT_TRUE(with_k.has_value());
  ASSERT_TRUE(without_k.has_value());
  EXPECT_EQ(*with_k, *without_k);
}

TEST(MatcherBuilder, LookupUnknownReturnsNullopt) {
  EXPECT_FALSE(LookupNodeEnum("kThisDoesNotExist").has_value());
  EXPECT_FALSE(LookupNodeEnum("").has_value());
  EXPECT_FALSE(LookupNodeEnum("random_garbage").has_value());
}

TEST(MatcherBuilder, TokenEnumStubAlwaysNullopt) {
  EXPECT_FALSE(LookupTokenEnum("any_name").has_value());
}

}  // namespace
}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
