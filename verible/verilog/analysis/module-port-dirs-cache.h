// Copyright 2024 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_MODULE_PORT_DIRS_CACHE_H_
#define VERIBLE_VERILOG_ANALYSIS_MODULE_PORT_DIRS_CACHE_H_

#include <map>
#include <string>

namespace verilog {
namespace analysis {

// Global cache for module port directions detected by Language Server.
// This allows lint rules (e.g., GJB-10157-R-3-2) to access port direction
// information from modules defined in OTHER files within the same project.
//
// The cache is populated by the Language Server's SymbolTableHandler after
// building the project symbol table, by scanning all module declarations
// across all files and extracting port directions.
//
// Usage pattern:
//   - Language Server calls SetPortDirection() after project analysis
//   - Lint rules call GetPortDirection() to check port directions
class ModulePortDirsCache {
 public:
  static ModulePortDirsCache& GetInstance();

  // Set port direction for a module.
  // direction should be "input", "output", or "inout".
  void SetPortDirection(const std::string& module_name,
                        const std::string& port_name,
                        const std::string& direction);

  // Get the direction of a port in a module.
  // Returns empty string if the module or port is not found.
  std::string GetPortDirection(const std::string& module_name,
                               const std::string& port_name) const;

  // Check if a module exists in the cache.
  bool HasModule(const std::string& module_name) const;

  // Check if the cache has any data.
  bool HasData() const;

  // Clear the cache.
  void Clear();

 private:
  ModulePortDirsCache() = default;
  // module_name -> {port_name -> direction}
  std::map<std::string, std::map<std::string, std::string>> module_ports_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_MODULE_PORT_DIRS_CACHE_H_
