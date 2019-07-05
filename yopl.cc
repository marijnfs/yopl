#include "../yagll/yagll.h"
#include "print.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <memory>
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
    cout << pg.type(n) << "(" << pg.text(n) << ")" << endl;
  else
    cout << pg.type(n) << endl;

  for (auto c : pg.children(n))
    tree_print(pg, c, depth + 1); 
}

typedef vector<variant<llvm::Value*, llvm::Type*>> ValueVector;
typedef map<string, llvm::Value*> ValueMap;

struct Context {
  map<string, llvm::Value*> value_map;
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

  void add_value(string name, Value *value) {
    value_map[name] = value;
  }
};

struct ModuleBuilder {
  LLVMContext &C;
  unique_ptr<llvm::Module> module;
  // unique_ptr<BasicBlock> block;
  Context context;
  ValueVector value_vector;

  ModuleBuilder(LLVMContext &C_, string name) : C(C_), module(nullptr) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    module.reset(new llvm::Module(name, C));
  }

  void process_parsegraph(ParseGraph &pg) {
    value_vector.resize(pg.size());
    process_module(pg, 0);
  }

  void process_module(ParseGraph &pg, int n) {
    std::vector<Type *> inputs;
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), inputs, false);
    Function *main_func =
      Function::Create(FT, Function::ExternalLinkage, "main", module.get());

    main_func->setCallingConv(CallingConv::C);


    auto block = llvm::BasicBlock::Create(C, "entry", main_func);
    llvm::IRBuilder<> builder(block);
    
    auto rulename = pg.type(n);

    cout << "Building function: " << endl;
    pg.visit_dfs_filtered(n, bind(&ModuleBuilder::process_lines, this, _1, _2, &context, &builder, main_func));
    // ReturnInst::Create(C, llvm::ConstantFP::get(C, APFloat(2.0)), block);

    main_func->print(outs());
    std::string errStr;
    if (verifyFunction(*main_func, &outs())) {
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

    typedef double MainFunc();
    MainFunc *f = (MainFunc*) EE->getFunctionAddress("main");
    println(EE->getFunctionAddress("main"));
    println("func: ptr", f);
    double result = f();
    println(result);
  }

  bool process_lines(ParseGraph &pg, int n, Context *context, llvm::IRBuilder<> *builder, llvm::Function *cur_func) {
    auto rulename = pg.type(n);
    if (rulename == "line") {
      println("+exp bottom up start");
      pg.visit_bottom_up(n, bind(&ModuleBuilder::process_exp, this, _1, _2, context->value_map, builder));
      return false;
    }
    if (rulename == "branch") {
      int condition_n = pg.children(n)[0];
      print("cond text:", pg.text(condition_n));
      pg.visit_bottom_up(condition_n, bind(&ModuleBuilder::process_exp, this, _1, _2, context->value_map, builder));

      auto cond_val = get<llvm::Value*>(value_vector[condition_n]);
      int branch_n = pg.get_one(n, set<string>{"if", "ifelse"});

      if (pg.type(branch_n) == "if") { //only one branch
        print("Single val");
        print("cond val ", cond_val);
        auto if_block = llvm::BasicBlock::Create(C, "if", cur_func);
        auto continued = llvm::BasicBlock::Create(C, "continued", cur_func);

        print("> branch ", if_block, " ", continued);
        builder->CreateCondBr(cond_val, if_block, continued);


        builder->SetInsertPoint(if_block);

        pg.visit_dfs_filtered(branch_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context, builder, cur_func));

        if (builder->GetInsertBlock()->getTerminator() == nullptr)
          builder->CreateBr(continued);
        builder->SetInsertPoint(continued);
      }

      if (pg.type(branch_n) == "ifelse") { //two branches
        int if_block_n = pg.children(branch_n)[0];
        int else_block_n = pg.children(branch_n)[1];

        auto if_block = llvm::BasicBlock::Create(C, "", cur_func);
        auto else_block = llvm::BasicBlock::Create(C, "", cur_func);
        auto continued = llvm::BasicBlock::Create(C, "", cur_func);

        builder->CreateCondBr(cond_val, if_block, else_block);

        builder->SetInsertPoint(if_block);

        pg.visit_dfs_filtered(if_block_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context, builder, cur_func));
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
          print(">>had no terminator");
          builder->CreateBr(continued);
        }

        builder->SetInsertPoint(else_block);
        print(">>>");
        pg.visit_dfs_filtered(else_block_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context, builder, cur_func));
        if (else_block->getTerminator() == nullptr)
          builder->CreateBr(continued);
        print("<<<");

        builder->SetInsertPoint(continued);
      }

      return false;
    }

    if (rulename == "classdef") {
      int name_n = pg.children(n)[0];
      string class_name = pg.text(name_n);
      println("class ", class_name);
      pg.visit_dfs(n, bind(&ModuleBuilder::process_classdef, this, _1, _2));
      return false;
    }

    if (rulename == "functiondef") {
      int ins_n  = pg.children(n)[0];
      int name_n = pg.children(n)[1];
      int outs_n = pg.children(n)[2];
      int body_n = pg.children(n)[3];
      
      auto func_name = pg.text(name_n);

      pg.visit_bottom_up(ins_n, bind(&ModuleBuilder::process_type, this, _1, _2 ));
      std::vector<Type*> inputs;
      for (auto c : pg.get_all(ins_n, "vardef")) { //also allow for others later (name only)
          inputs.push_back(get<llvm::Type*>(value_vector[c]));
      }

      // Function *mul_add = mod->getFunction("mul_add");
      auto retType = Type::getInt32Ty(C);
      FunctionType *FT = FunctionType::get(retType, inputs, false);
      Function *func =
          Function::Create(FT, Function::ExternalLinkage, func_name, module.get());
      // Function *mul_add = cast<Function>(mod->getOrInsertFunction("muladd", FT));
      func->setCallingConv(CallingConv::C);

      auto args = func->arg_begin();
      for (auto c : pg.get_all(ins_n, "vardef")) { //also allow for others later (name only)
        auto name_n = pg.get_one(c, "name");
        if (name_n < 0)
          continue;
        Argument *a = &*args++;
        a->setName(pg.text(name_n));
      }

      BasicBlock *block = BasicBlock::Create(C, "entry", func, 0);
      process_block(pg, body_n, builder);
      print("> > function: ", func_name, " ", pg.text(ins_n), " > ", pg.text(outs_n));
      return false;
    }
    return true;
  }

  void process_block(ParseGraph &pg, int n, llvm::IRBuilder<> *builder) {

  }


  void process_exp(ParseGraph &pg, int n, ValueMap &value_map, llvm::IRBuilder<> *builder) {
    auto rulename = pg.type(n);
    println("exp: ", rulename);

    if (rulename == "number") {
      double val(0);
      istringstream iss(pg.text(n));
      iss >> val;
      value_vector[n] = llvm::ConstantFP::get(C, APFloat(val));
      println("> created constant ", val, " ", get<Value*>(value_vector[n]));
    } 
    else if (rulename == "times") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      println("> adding fmul ", get<llvm::Value*>(value_vector[c1]), " ", get<llvm::Value*>(value_vector[c2]));
      value_vector[n] = builder->CreateFMul(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "divide") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      println("> adding fdiv ", get<llvm::Value*>(value_vector[c1]), " ", get<llvm::Value*>(value_vector[c2]));
      value_vector[n] = builder->CreateFDiv(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "plus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      println("> adding fadd ", get<llvm::Value*>(value_vector[c1]), " ", get<llvm::Value*>(value_vector[c2]));
      value_vector[n] = builder->CreateFAdd(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "minus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      println("> adding fsub ", get<llvm::Value*>(value_vector[c1]), " ", get<llvm::Value*>(value_vector[c2]));
      value_vector[n] = builder->CreateFSub(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "neg") {
      int c1 = pg.children(n)[0];
      println("> adding neg ", get<llvm::Value*>(value_vector[c1]));
      value_vector[n] = builder->CreateFMul(get<llvm::Value*>(value_vector[c1]), llvm::ConstantFP::get(C, APFloat(-1.0)));
    }
    else if (rulename == "loadvarptr") {
      auto var_name = pg.text(n);
      auto value_ptr = context.get_value(var_name);
      if (!value_ptr) {
        value_ptr = builder->CreateAlloca(Type::getDoubleTy(C), 0, var_name);
        println("> adding alloca: ", var_name, " ", value_ptr);
        context.add_value(var_name, value_ptr);
      } 
      value_vector[n] = value_ptr;
    }
    else if (rulename == "stat") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      auto value_ptr = get<llvm::Value*>(value_vector[c2]);
      auto target_ptr = get<llvm::Value*>(value_vector[c1]);
      println("> adding storeinst ", pg.text(c1), " to ", get<llvm::Value*>(value_vector[c1]));
      value_vector[n] = builder->CreateStore(value_ptr, target_ptr);
    }
    else if (rulename == "loadvar") {
      int c1 = pg.children(n)[0];
      auto var_name = pg.text(c1);
      auto value_ptr = context.get_value(var_name);
      if (!value_ptr) {
        cerr << "no variable called " << var_name << endl;
        return;
      }
      println("> adding loadinst ", var_name, " ", value_ptr);
      value_vector[n] = builder->CreateLoad(value_ptr, false);
    }
    else if (rulename == "ret") {
      int c1 = pg.children(n)[0];
      println("> adding ret");
      builder->CreateRet(get<llvm::Value*>(value_vector[c1]));
    }
    else if (rulename == "inc") {
      int c1 = pg.children(n)[0];
      auto var_name = pg.text(c1);
      auto value_ptr = get<llvm::Value*>(value_vector[c1]);
      auto load = builder->CreateLoad(value_ptr, false);
      auto constant = llvm::ConstantFP::get(C, APFloat(1.0));
      println("> adding fadd ", value_ptr, " ", constant);

      auto add_inst = builder->CreateFAdd(load, constant);
      println("> adding store ", add_inst);
      builder->CreateStore(add_inst, value_ptr);
      value_vector[n] = add_inst;
    }
    else if (rulename == "dec") {
      int c1 = pg.children(n)[0];
      auto var_name = pg.text(c1);
      auto value_ptr = get<llvm::Value*>(value_vector[c1]);
      auto load = builder->CreateLoad(value_ptr, false);
      auto add_inst = builder->CreateFSub(load, llvm::ConstantFP::get(C, APFloat(1.0)));
      builder->CreateStore(add_inst, value_ptr);
      value_vector[n] = add_inst;
    }
    else if (rulename == "less") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateFCmpULT(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "more") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      print("creating FCmpUGT");
      value_vector[n] = builder->CreateFCmpUGT(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
      print(get<llvm::Value*>(value_vector[n]));
    }
    else if (rulename == "lesseq") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateFCmpULE(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "moreeq") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateFCmpUGE(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "noteq") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateFCmpUNE(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "eq") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateFCmpUEQ(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "and") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateAnd(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else if (rulename == "or") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      value_vector[n] = builder->CreateOr(get<llvm::Value*>(value_vector[c1]), get<llvm::Value*>(value_vector[c2]));
    }
    else {
      //for all other nodes, just propagate the ptr
      propagate(pg, n);
    }
  }

  void process_type(ParseGraph &pg, int n) {
    auto rulename = pg.type(n);
    if (rulename == "basetype") {
      auto basetypen = pg.children(n)[0];
      auto basetypename = pg.text(basetypen);
      bool is_ptr = pg.get_one(n, "noptr") == -1;
      if (!is_ptr) {
        if (basetypename == "i64")
          value_vector[n] = llvm::Type::getInt64Ty(C);
        if (basetypename == "i32")
          value_vector[n] = llvm::Type::getInt32Ty(C);
        if (basetypename == "i16")
          value_vector[n] = llvm::Type::getInt16Ty(C);
        if (basetypename == "f32")
          value_vector[n] = llvm::Type::getFloatTy(C);
        if (basetypename == "f64")
          value_vector[n] = llvm::Type::getDoubleTy(C);
      }
    } else {
      propagate(pg, n);
    }
  }

  void process_classdef(ParseGraph &pg, int n) {
    
  }

  void propagate(ParseGraph &pg, int n) {
    if (pg.children(n).size() == 0) {
      println("no children to propagate ", n, " type ", pg.type(n));
      return;
    }
    value_vector[n] = value_vector[pg.children(n)[0]];
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
    println("Grammar file doesn't exist");
    return -1;
  }
  if (!fs::exists(input_path)) {
    println("Input file doesn't exist");
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
        println("func", pg.text(pg.children(n)[1]));
        pg.visit_dfs_filtered(n, [](ParseGraph &pg, int n) -> bool {
          if (pg.type(n) == "name")
            println(pg.text(n));
          return true;
        });
        return false;
    }

    return true;
  });

  LLVMContext C;
  ModuleBuilder module_builder(C, "module");
  module_builder.process_parsegraph(*parse_graph);

}
