#pragma once

#include <map>
#include <string>
#include <iostream>

using std::string;

struct Context {
  std::map<string, llvm::Value*> value_map;
  std::map<string, llvm::Type*>  type_map;
  
  //std::map<string, StructDefinition*> struct_map;

  Context *parent = nullptr;

  Context(Context *parent_ = nullptr) : parent(parent_) {}

  llvm::Value* get_value(string name) {
    println("getval: ", name);
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

  //void add_struct(string name, StructDefinition *struct_def) {
  //  struct_map[name] = struct_def;
  //}

};
