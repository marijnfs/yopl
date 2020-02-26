#pragma once

#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include "print.h"

using std::string;

struct Context {
  std::map<string, llvm::Value*> value_map;
  std::map<string, llvm::Type*>  type_map;
    llvm::BasicBlock *break_block = nullptr;
    llvm::BasicBlock *continue_block = nullptr;

    std::map<string, std::map<string, int>> member_lookup;

  //std::map<string, StructDefinition*> struct_map;

  Context *parent = nullptr;

  Context(Context *parent_ = nullptr) : parent(parent_) {}

  llvm::Value* get_value(string name) {
    if (value_map.count(name))
      return value_map[name];
    if (parent)
      return parent->get_value(name);
    return nullptr;
  }

  void add_value(string name, llvm::Value *value) {
    value_map[name] = value;
  }

  void add_type(string name, llvm::Type *type) {
    type_map[name] = type;
  }

    void add_struct(string name, llvm::Type *type, std::vector<std::string> variable_names) {
        type_map[name] = type;
        std::map<string, int> lookup_map;
        for (int n(0); n < variable_names.size(); ++n)
            lookup_map[variable_names[n]] = n;
        member_lookup[name] = lookup_map;
    }

  int get_struct_member_index(string structname, string membername) {
    if (member_lookup.count(structname) == 0) {
      std::ostringstream oss;
      oss << "No such struct " << structname;
      throw std::runtime_error(oss.str());
    }
    if (member_lookup[structname].count(membername) == 0) {
      std::ostringstream oss;
      oss << "struct " << structname << " does not have member " << membername;
      throw std::runtime_error(oss.str());
    }
    return member_lookup[structname][membername];
  }
    
    llvm::BasicBlock *get_break() {
        if (break_block)
            return break_block;
        if (parent)
            return parent->get_break();
        return nullptr;
    }

    llvm::BasicBlock *get_continue() {
        if (continue_block)
            return continue_block;
        if (parent)
            return parent->get_continue();
        return nullptr;
    }

  //void add_struct(string name, StructDefinition *struct_def) {
  //  struct_map[name] = struct_def;
  //}

};
