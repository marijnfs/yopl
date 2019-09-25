#pragma once
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
/*

using namespace std;
using namespace llvm;
using namespace std::placeholders;  // for _1, _2, _3...

struct StructDefinition {
  string name;
  map<string, int> member_index;
  vector<Type*> types;

  Type* type;

  void add_type(string member, Type *type) {
    member_index[member] = types.size();
    types.push_back(type);
  }

  void build_type() {
    type = StructType::create(types, name);
  }
};

*/
