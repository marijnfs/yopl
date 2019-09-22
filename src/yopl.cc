#include <yagll.h>
#include "print.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <memory>
#include <experimental/filesystem>

#include "llvm_builder.h"
#include "yopl.h"
#include "context.h"

using namespace std;
typedef vector<variant<llvm::Value*, llvm::Type*>> ValueVector;
typedef map<string, llvm::Value*> ValueMap;


struct ModuleBuilder {
  LLVMContext &C;
  unique_ptr<llvm::Module> module;
  // unique_ptr<BasicBlock> block;
  Context context;
  ValueVector value_vector;

  ModuleBuilder(LLVMContext &C_, string name) : C(C_), module(nullptr) {
  }

  void process_parsegraph(ParseGraph &pg) {
  }

  bool process_arguments(ParseGraph &pg, int n, vector<string> *names, vector<Type*> *types) {

    return true;
  }

  void process_module(ParseGraph &pg, int n) {
  }

  void process_function(ParseGraph &pg, int n, Context *context) {
  }

  bool process_lines(ParseGraph &pg, int n, Context *outer_context, llvm::IRBuilder<> *builder, llvm::Function *cur_func) {
    auto context = unique_ptr<Context>(new Context(outer_context));
    auto rulename = pg.type(n);
    if (rulename == "line") {
      println("+exp bottom up start");
      pg.visit_bottom_up(n, bind(&ModuleBuilder::process_exp, this, _1, _2, context.get(), builder));
      return false;
    }
    if (rulename == "branch") {
      int condition_n = pg.children(n)[0];
      print("cond text:", pg.text(condition_n));
      pg.visit_bottom_up(condition_n, bind(&ModuleBuilder::process_exp, this, _1, _2, context.get(), builder));

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

        pg.visit_dfs_filtered(branch_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context.get(), builder, cur_func));

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

        pg.visit_dfs_filtered(if_block_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context.get(), builder, cur_func));
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
          print(">>had no terminator");
          builder->CreateBr(continued);
        }

        builder->SetInsertPoint(else_block);
        print(">>>");
        pg.visit_dfs_filtered(else_block_n, bind(&ModuleBuilder::process_lines, this, _1, _2, context.get(), builder, cur_func));
        if (else_block->getTerminator() == nullptr)
          builder->CreateBr(continued);
        print("<<<");

        builder->SetInsertPoint(continued);
      }

      return false;
    }

    if (rulename == "structdef") {
      int name_n = pg.children(n)[0];
      string class_name = pg.text(name_n);
      println("class ", class_name);
      process_structdef(pg, n);
      // pg.visit_dfs(n, bind(&ModuleBuilder::process_structdef, this, _1, _2));
      return false;
    }

    if (rulename == "functiondef") {

      // process_function(pg, n, context.get()); //initially we don't have global vars, so no context
      process_function(pg, n, nullptr);
      // int ins_n  = pg.children(n)[0];
      // int name_n = pg.children(n)[1];
      // int outs_n = pg.children(n)[2];
      // int body_n = pg.children(n)[3];
      
      // auto func_name = pg.text(name_n);

      // pg.visit_bottom_up(ins_n, bind(&ModuleBuilder::process_type, this, _1, _2 ));
      // std::vector<Type*> inputs;
      // for (auto c : pg.get_all(ins_n, "vardef")) { //also allow for others later (name only)
      //     inputs.push_back(get<llvm::Type*>(value_vector[c]));
      // }

      // // Function *mul_add = mod->getFunction("mul_add");
      // auto retType = Type::getInt32Ty(C);
      // FunctionType *FT = FunctionType::get(retType, inputs, false);
      // Function *func =
      //     Function::Create(FT, Function::ExternalLinkage, func_name, module.get());
      // // Function *mul_add = cast<Function>(mod->getOrInsertFunction("muladd", FT));
      // func->setCallingConv(CallingConv::C);

      // auto args = func->arg_begin();
      // for (auto c : pg.get_all(ins_n, "vardef")) { //also allow for others later (name only)
      //   auto name_n = pg.get_one(c, "name");
      //   if (name_n < 0)
      //     continue;
      //   Argument *a = &*args++;
      //   a->setName(pg.text(name_n));
      // }

      // BasicBlock *block = BasicBlock::Create(C, "entry", func, 0);
      // process_block(pg, body_n, builder);
      // print("> > function: ", func_name, " ", pg.text(ins_n), " > ", pg.text(outs_n));
      return false;
    }
    return true;
  }

  void process_block(ParseGraph &pg, int n, llvm::IRBuilder<> *builder) {

  }


  void process_exp(ParseGraph &pg, int n, Context *value_map, llvm::IRBuilder<> *builder) {
  }

  void process_type(ParseGraph &pg, int n) {
  }

  void process_structdef(ParseGraph &pg, int n) {
  }

  void propagate(ParseGraph &pg, int n) {
    if (pg.children(n).size() == 0) {
      println("no children to propagate ", n, " type: ", pg.type(n), " text: ", pg.text(n));
      return;
    }
    value_vector[n] = value_vector[pg.children(n)[0]];
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
     return name == "items" || name == "entries" || name == "structentries" || name == "nodes" || name == "flow" || name == "varname" || name == "sources";
    // return name == "items" || name == "entries" || name == "structentries" || name == "varname" || name == "sources";
  });

  parse_graph->sort_children([&parse_graph](int a, int b) -> bool {
    return parse_graph->starts[a] < parse_graph->starts[b];
  });

  // //Actually process the info, basic example printing class names
  // ParseGraph::BoolCallback cb([](ParseGraph &pg, int n) -> bool {
  //   auto name = pg.type(n);
  //   if (name == "structdef") {
  //     cout << "found class: " << pg.text(pg.children(n)[0]) << endl;
  //     return false;
  //   }
  //   return true;
  // });

  // parse_graph->visit_dfs(0, cb);
  parse_graph->print_dot("compact.dot");


  tree_print(*parse_graph, 0, 0);


  //Debug printing
  // parse_graph->visit_dfs_filtered(0, [](ParseGraph &pg, int n) -> bool {
  //   if (pg.type(n) == "functiondef") {
  //       println("func", pg.text(pg.children(n)[1]));
  //       pg.visit_dfs_filtered(n, [](ParseGraph &pg, int n) -> bool {
  //         if (pg.type(n) == "name")
  //           println(pg.text(n));
  //         return true;
  //       });
  //       return false;
  //   }

  //   return true;
  // });

  LLVMContext C;
  ModuleBuilder module_builder(C, "module");
  module_builder.process_parsegraph(*parse_graph);

}
