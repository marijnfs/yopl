#include "../yagll/yagll.h"
#include "print.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

#include <experimental/filesystem>

#include "llvm_builder.h"

namespace fs = std::experimental::filesystem;


using namespace std;
using namespace llvm;
using namespace std::placeholders;  // for _1, _2, _3...
 
void tree_print(ParseGraph &pg, int n, int depth) {
  for (int i(0); i < depth; ++i)
    cout << "  ";
  if (pg.type(n) == "name" || pg.type(n) == "varname" || pg.type(n) == "value" || pg.type(n) == "number")
    cout << "[" << pg.starts[n] << "] " << pg.type(n) << "(" << pg.text(n) << ")" << endl;
  else
    cout << pg.type(n) << endl;

  for (auto c : pg.children(n))
    tree_print(pg, c, depth + 1); 
}

typedef vector<variant<llvm::Value*, llvm::Type*>> ValueVector;
struct Context {

  ValueVector value_vector;
  map<string, llvm::Value*> value_map;
  Context *parent = nullptr;

  Context(Context *parent_ = nullptr) : parent(parent_) {}

  llvm::Value* get_value(string name) {
    if (value_map.count(name))
      return value_map[name];
    if (parent)
      return parent->get_value(name);
    return nullptr;
  }

  void add_value(string name, Value *value) {
    value_map[name] = value;
  }
};

struct ModuleBuilder {
  LLVMContext &C;
  unique_ptr<llvm::Module> module;
  // unique_ptr<BasicBlock> block;
  Context context;

  ModuleBuilder(LLVMContext &C_, string name) : C(C_), module(nullptr) {
    module.reset(new llvm::Module(name, C));
  }

  void process_module(ParseGraph &pg, int n) {
    std::vector<Type *> inputs;
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), inputs, false);
    Function *blafunc =
      Function::Create(FT, Function::ExternalLinkage, "bla", module.get());

    blafunc->setCallingConv(CallingConv::C);


    auto block = llvm::BasicBlock::Create(C, "entry", 0);
    auto rulename = pg.type(n);

    cout << "Building function: " << endl;

    pg.visit_dfs_filtered(n, bind(&ModuleBuilder::process_lines, this, _1, _2, context, block));
    ReturnInst::Create(C, llvm::ConstantFP::get(C, APFloat(0.0)), block);


    std::string errStr;
    if (verifyFunction(*blafunc, &outs())) {
      cout << "failed verification: " << errStr << endl;
      return;
    }


    ExecutionEngine *EE =
        EngineBuilder(std::move(module)).setErrorStr(&errStr).create();


    if (!EE) {
      errs() << ": Failed to construct ExecutionEngine: " << errStr
             << "\n";
      return;
    }


    EE->finalizeObject();

    typedef double bla();
    bla *f = (bla *)EE->getFunctionAddress("bla");
    double result = f();
    print(result);
  }

  bool process_lines(ParseGraph &pg, int n, Context &context, BasicBlock *block) {
    auto rulename = pg.type(n);
    if (rulename == "line") {
      pg.visit_bottom_up(n, bind(&ModuleBuilder::process_exp, this, _1, _2, context.value_vector, context.value_map, block));
      return false;
    }

    if (rulename == "classdef") {
      int name_n = pg.children(n)[0];
      string class_name = pg.text(name_n);
      print("class ", class_name);
      pg.visit_dfs(n, bind(&ModuleBuilder::process_classdef, this, _1, _2));
      return false;
    }
    return true;
  }

  void process_type(ParseGraph &pg, int n, ValueVector &val_vec) {
    auto rulename = pg.type(n);
    if (rulename == "basetype") {
      auto basetypen = pg.children(n)[0];
      auto basetypename = pg.text(basetypen);
      bool is_ptr = pg.get_one(n, "noptr") == -1;
      if (!is_ptr) {
        if (basetypename == "i64")
          val_vec[n] = llvm::Type::getInt64Ty(C);
        if (basetypename == "i32")
          val_vec[n] = llvm::Type::getInt32Ty(C);
        if (basetypename == "i16")
          val_vec[n] = llvm::Type::getInt16Ty(C);
        if (basetypename == "f32")
          val_vec[n] = llvm::Type::getFloatTy(C);
        if (basetypename == "f64")
          val_vec[n] = llvm::Type::getDoubleTy(C);
      }
    }
  }

  void process_classdef(ParseGraph &pg, int n) {
    
  }

  void process_exp(ParseGraph &pg, int n, ValueVector &val_vec, map<string, llvm::Value*> &value_map, BasicBlock *block) {
    auto rulename = pg.type(n);
    if (rulename == "number") {
      double val(0);
      istringstream iss(pg.text(n));
      iss >> val;
      val_vec[n] = llvm::ConstantFP::get(C, APFloat(val));
    } 
    else if (rulename == "times") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = llvm::BinaryOperator::CreateFMul(get<llvm::Value*>(val_vec[c1]), get<llvm::Value*>(val_vec[c2]), "", block);
    } 
    else if (rulename == "divide") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = llvm::BinaryOperator::CreateFDiv(get<llvm::Value*>(val_vec[c1]), get<llvm::Value*>(val_vec[c2]), "", block);
    } 
    else if (rulename == "plus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = llvm::BinaryOperator::CreateFAdd(get<llvm::Value*>(val_vec[c1]), get<llvm::Value*>(val_vec[c2]), "", block);
    }
    else if (rulename == "minus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = llvm::BinaryOperator::CreateFSub(get<llvm::Value*>(val_vec[c1]), get<llvm::Value*>(val_vec[c2]), "", block);

    } 
    else if (rulename == "stat") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      auto var_name = pg.text(c1);
      auto value_ptr = context.get_value(var_name);
      if (!value_ptr) {
        value_ptr = new AllocaInst(Type::getDoubleTy(C), 0, var_name, block);
        context.add_value(var_name, value_ptr);
      }
      val_vec[n] = new llvm::StoreInst(get<llvm::Value*>(val_vec[c2]), value_ptr, block);
    }
    else if (rulename == "var") {
      int c1 = pg.children(n)[0];
      auto var_name = pg.text(c1);
      auto value_ptr = context.get_value(var_name);
      if (!value_ptr) {
        cerr << "no variable called " << var_name << endl;
        return;
      }
      val_vec[n] = new llvm::LoadInst(value_ptr, 0, block);
    }

  }

};

struct Function {

  Function() {

  }


};


int main(int argc, char **argv) {
  cout << "yopl" << endl;
  if (argc < 3) {
    cerr << "not enough inputs, use: " << argv[0] << " [gram] [input]" << endl;
    return 1;
  }

  fs::path gram_path(argv[1]);
  fs::path input_path(argv[2]);

  if (!fs::exists(gram_path)) {
    print("Grammar file doesn't exist");
    return -1;
  }
  if (!fs::exists(input_path)) {
    print("Input file doesn't exist");
    return -1;
  }

  for (auto file : fs::directory_iterator(input_path.parent_path())) {
    cout << file << endl;
  }

  ifstream gram_file(gram_path);
  ifstream input_file(input_path);

  Parser parser(gram_file);
  auto parse_graph = parser.parse(input_file);
  
  if (!parse_graph)
    return 0;

  //filter empty lines and empty comments
  cout << "nodes: " << parse_graph->size() << endl;
  parse_graph->filter([](ParseGraph &pg, int n) -> bool {
    if (pg.type(n) == "entryln")
      return true;
    if (pg.type(n) == "nocomment" && pg.text(n) == "")
      return true;
    return false;        
  });

  //remove empty stuff
  parse_graph->remove([](ParseGraph &pg, int n) -> bool {
    if (pg.type(n) == "entry" && pg.get_one(n, "empty") != -1)
      return true;
    if (pg.type(n) == "optcom" && pg.text(n).size() == 0)
      return true;
    return false;
  });

  //squeeze recursive multi-items
  parse_graph->squeeze([](ParseGraph &pg, int n) -> bool {
    auto name = pg.type(n);
     return name == "items" || name == "entries" || name == "clentries" || name == "nodes" || name == "flow" || name == "varname" || name == "sources";
    // return name == "items" || name == "entries" || name == "clentries" || name == "varname" || name == "sources";
  });


  parse_graph->sort_children([&parse_graph](int a, int b) -> bool {
    return parse_graph->starts[a] < parse_graph->starts[b];
  });

  //Actually process the info
  ParseGraph::BoolCallback cb([](ParseGraph &pg, int n) -> bool {
    auto name = pg.type(n);
    if (name == "classdef") {
      cout << "found class: " << pg.text(pg.children(n)[0]) << endl;
      return false;
    }
    return true;
  });

  parse_graph->visit_dfs(0, cb);
  parse_graph->print_dot("compact.dot");


  tree_print(*parse_graph, 0, 0);


  parse_graph->visit_dfs_filtered(0, [](ParseGraph &pg, int n) -> bool {
    if (pg.type(n) == "functiondef") {
        print(pg.type(n));
        print("func", pg.text(pg.children(n)[1]));
        pg.visit_dfs_filtered(n, [](ParseGraph &pg, int n) -> bool {
          if (pg.type(n) == "name")
            print(pg.text(n));
          return true;
        });
        return false;
    }

    return true;
  });

  LLVMContext C;
  ModuleBuilder module_builder(C, "module");
  module_builder.process_module(*parse_graph, 0);

}
