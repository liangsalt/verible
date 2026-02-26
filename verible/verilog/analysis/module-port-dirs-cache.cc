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

#include "verible/verilog/analysis/module-port-dirs-cache.h"

#include <map>
#include <string>

namespace verilog {
namespace analysis {

ModulePortDirsCache& ModulePortDirsCache::GetInstance() {
  static ModulePortDirsCache instance;
  return instance;
}

void ModulePortDirsCache::SetPortDirection(const std::string& module_name,
                                            const std::string& port_name,
                                            const std::string& direction) {
  module_ports_[module_name][port_name] = direction;
}

std::string ModulePortDirsCache::GetPortDirection(
    const std::string& module_name, const std::string& port_name) const {
  auto mod_it = module_ports_.find(module_name);
  if (mod_it == module_ports_.end()) return "";
  auto port_it = mod_it->second.find(port_name);
  if (port_it == mod_it->second.end()) return "";
  return port_it->second;
}

bool ModulePortDirsCache::HasModule(const std::string& module_name) const {
  return module_ports_.find(module_name) != module_ports_.end();
}

bool ModulePortDirsCache::HasData() const {
  return !module_ports_.empty();
}

void ModulePortDirsCache::Clear() {
  module_ports_.clear();
}

}  // namespace analysis
}  // namespace verilog
