#include "llvm/ADT/APInt.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Bitcode/BitcodeWriter.h"

#include <map>
#include <string>
#include <functional>
#include <stdexcept>
#include <yagll.h>
#include "context.h"

using namespace std::placeholders;  // for _1, _2, _3...                        

struct NodeBuilder {
  llvm::LLVMContext &C;
  llvm::Module &module;
  ValueVector &value_vector;

  std::shared_ptr<Context> context;
  std::shared_ptr<Function> current_func; 

  llvm::Value* llvm_value(int n) {
    return get<llvm::Value*>(value_vector[n]);
  }

  llvm::Value* llvm_type(int n) {
    return get<llvm::Type*>(value_vector[n]);
  }
};

struct ModuleBuilder : NodeBuilder {
  ModuleBuilder();
  
  void f() {
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
    if (verifyFunction(*main_func, &outs())) {
      cout << "Function failed verification: " << endl;
      return;
    }

    if (verifyModule(*module, &outs())) {
      cout << "Module failed verification: " << endl;
      return;      
    }

    module->print(outs(), nullptr);


    std::string errStr;
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
    // println(EE->getFunctionAddress("main"));
    println("func: ptr", f);
    double result = f();
    println(result);
  }
};

struct ExpBuilder : NodeBuilder, BottomUpGraphCallback {
  ExpBuilder() {
    register_callback("number", std::bind(&ExpBuilder::p_number, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_times, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_divide, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_add, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_subtract, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_neg, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_loadvarptrcreate, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_loadvarptr, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_loadvar, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_statement, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_return, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_inc, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_dec, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_less, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_more, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_less_eq, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_more_eq, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_eq, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_neq, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_and, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_or, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_, this, _1));
    register_callback("", std::bind(&ExpBuilder::p_, this, _1));


  }
  
  void p_number(int n) {
    SearchNode node{n, pg};
    
    value_vector[n] = llvm::ConstantFP::get(C, APFloat(node.text_to<double>()));
  }
  
  void p_times(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector = builder->CreateFMul(llvm_value(c0), llvm_value(c1));
  }
  
  void p_divide(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector = builder->CreateFDiv(llvm_value(c0), llvm_value(c1));
   }

  void p_add(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector = builder->CreateFAdd(llvm_value(c0), llvm_value(c1));
  }

  void p_subtract(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector = builder->CreateFSub(llvm_value(c0), llvm_value(c1));
  }

  void p_neg(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    value_vector = builder->CreateFSub(llvm_value(c0), llvm::ConstantFP::get(C, APFloat(-1.0)));
  }

  void p_loadvarptrcreate(int n) {
    SearchNode node{n, pg};
    auto var_name = node.text();
    auto value_ptr = context.get_value(var_name);
    if (!value_ptr) {
      value_ptr = builder->CreateAlloca(Type::getDoubleTy(C), 0, var_name);
      context.add_value(var_name, value_ptr);
    }
    value_vector[n] = value_ptr;
  }

  void p_loadvarptr(int n) {
    SearchNode node{n, pg};
    auto var_name = node.text();
    auto value_ptr = context.get_value(var_name);
    if (!value_ptr) {
      print("couldn't locate ", var_name);
      throw std::runtime_error("undefined variable error");
    }
    value_vector[n] = value_ptr;
  }

  void p_loadvar(int n) {
    SearchNode node{n, pg};
    auto var_name = node.child().text();
    auto value_ptr = context.get_value(var_name);
    if (!value_ptr) {
      cerr << "no variable called " << var_name << endl;
      throw std::runtime_error();
    }
    value_vector[n] = builder->CreateLoad(value_ptr, false);
  }
  
  void p_statement(int n) {
    SearchNode node{n, pg};
    int c0 = node.child(0);
    int c1 = node.child(1);
    auto target_ptr = get_llvm(c0);
    auto value = get_llvm(c1);
    value_vector[n] = builder->CreateStore(value_ptr, target_ptr);
  }
  
  void p_return(int n) {
    SearchNode node{n, pg};
    builder->CreateRet(llvm_value(node.child()));
  }
  
  void p_inc(int n) {
    SearchNode node{n, pg};
    auto value_ptr = llvm_value(node.child(0).N);
    auto load = builder->CreateLoad(value_ptr, false);
    auto constant = llvm::ConstantFP::get(C, APFloat(1.0));
    auto add_inst = builder->CreateFAdd(load, constant);
    builder->CreateStore(add_inst, value_ptr);
    value_vector[n] = add_inst;
  }

  void p_dec(int n) {
    SearchNode node{n, pg};
    auto value_ptr = llvm_value(node.child(0).N);
    auto load = builder->CreateLoad(value_ptr, false);
    auto constant = llvm::ConstantFP::get(C, APFloat(1.0));
    auto sub_inst = builder->CreateFSub(load, constant);
    builder->CreateStore(sub_inst, value_ptr);
    value_vector[n] = sub_inst;
  }

  void p_less(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpULT(llvm_value(c0), llvm_value(c1));
  }

  void p_more(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpUGT(llvm_value(c0), llvm_value(c1));
  }

  void p_less_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpULE(llvm_value(c0), llvm_value(c1));
  }

  void p_more_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpUGE(llvm_value(c0), llvm_value(c1));
  }

  void p_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpUEQ(llvm_value(c0), llvm_value(c1));
  }

  void p_neq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateFCmpUNE(llvm_value(c0), llvm_value(c1));
  }

  void p_and(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateAnd(llvm_value(c0), llvm_value(c1));
  }

  void p_or(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0);
    auto c1 = node.child(1);
    value_vector[n] = builder->CreateOr(llvm_value(c0), llvm_value(c1));
  }

  void run_default(int n) {
    SearchNode node{n, pg};
    value_vector[n] = value_vector[node.child().N];
  }
};

struct BlockBuilder : NodeBuilder, TypeGraphCallback {
  BlockBuilder() {
    register_callback("line", std::bind(&BlockBuilder::p_line, this, _1));
    register_callback("branch", std::bind(&BlockBuilder::p_branch, this, _1));
  }


  void p_line(int n) {
    ExpBuilder exp_builder;
    pg.visit_bottom(n, exp_builder);
  }

  void p_branch(int n) {
    auto context = unique_ptr<Context>(new Context(outer_context));
    SearchNode node{n, pg};

    auto condition = node.child("condition");
    auto body = node.child("body");
    
    ExpBuilder exp_builder;
    condition.bottom_up(exp_builder);
    auto cond_val = get<llvm::Value*>(value_vector[condition.N]); //to search node?
    
    auto continued = llvm::BasicBlock::Create(C, "continued", cur_func);
    auto if_block = llvm::BasicBlock::Create(C, "if", cur_func);
    
    auto branchblock_node = body.child("branchblock").child();
    
    if (branchblock_node.type() == "if") {
      builder->CreateCondBr(cond_val, if_block, continued);
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder;
      branch_block.child().top_down(block_builder);
      
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      builder->SetInsertPoint(continued);
      
    } else if (branchblock_node.type() == "ifelse") {
      auto else_block = llvm::BasicBlock::Create(C, "else", cur_func);
      builder->CreateCondBr(cond_val, if_block, else_block);
      
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder;
      branchblock_node.child(0).top_down(block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      
      builder->SetInsertPoint(else_block);
      BlockBuilder block_builder;
      branchblock_node.child(1).top_down(block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      
      builder->SetInsertPoint(continued);
    }
  }
};

struct TypeBuilder : NodeBuilder, BottomUpGraphCallback {
  TypeBuilder() {
    register_callback("i64", std::bind(&TypeBuilder::p_i64, this, _1));
    register_callback("i32", std::bind(&TypeBuilder::p_i32, this, _1));
    register_callback("i16", std::bind(&TypeBuilder::p_i16, this, _1));
    register_callback("f64", std::bind(&TypeBuilder::p_f64, this, _1));
    register_callback("f32", std::bind(&TypeBuilder::p_f32, this, _1));
    register_callback("ptrof", std::bind(&TypeBuilder::p_ptrof, this, _1));
  }

  void p_i64(int n) {
    value_vector[n] = llvm::Type::getInt64Ty(C);
  }

  void p_i32(int n) {
    value_vector[n] = llvm::Type::getInt32Ty(C);
  }

  void p_i16(int n) {
    value_vector[n] = llvm::Type::getInt16Ty(C);
  }

  void p_f32(int n) {
    value_vector[n] = llvm::Type::getFloatTy(C);
  }

  void p_f64(int n) {
    value_vector[n] = llvm::Type::getDoubleTy(C);
  }

  void p_ptrof(int n) {
    SearchNode node{n, pg};
    value_vector[n] = PointerType::get(llvm_type(node.child().N), 0);
  }
  
  void run_default(int n) {
    SearchNode node{n, pg};
    value_vector[n] = value_vector[node.child().N];
  }
};

struct FunctionBuilder : NodeBuilder {
  FunctionBuilder() {
  }

  void p_function(int n) {
    SearchNode node{n, pg};

    ArgsBuilder args_builder;
    node.child("in").bottom_up(args_builder);
    
    auto func_name = node.child("name").text();
    auto out = node.child("out");

    auto body = node.child("body");

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), arg_builder.types, false);

    Function *new_func =
      Function::Create(FT, Function::ExternalLinkage, funcname, module.get());
    auto block = llvm::BasicBlock::Create(C, "entry", new_func);
    llvm::IRBuilder<> builder(block);

    auto func_context = unique_ptr<Context>(new Context(context));
    pg.visit_dfs_filtered(entries_n, bind(&ModuleBuilder::process_lines, this, _1, _2, func_context.get(), &builder, new_func));

    print("printing new func:");
    new_func->print(outs());
    print("checking:");
    
    if (verifyFunction(*new_func, &outs())) {
      cout << "Defined Function failed verification: " << endl;
      exit(1);
    }
    
  }


  void f() {
    int input_n  = pg.children(n)[0];
    int name_n   = pg.children(n)[1];
    int output_n = pg.children(n)[2];
    int entries_n = pg.children(n)[3];

    vector<string> names;
    vector<Type*> types;
    print("Processing arguments");
    pg.visit_dfs_filtered(input_n, bind(&ModuleBuilder::process_arguments, this, _1, _2, &names, &types));
    print("Done");

    auto funcname = pg.text(name_n);

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), types, false);

    Function *new_func =
      Function::Create(FT, Function::ExternalLinkage, funcname, module.get());
    auto block = llvm::BasicBlock::Create(C, "entry", new_func);
    llvm::IRBuilder<> builder(block);

    auto func_context = unique_ptr<Context>(new Context(context));
    pg.visit_dfs_filtered(entries_n, bind(&ModuleBuilder::process_lines, this, _1, _2, func_context.get(), &builder, new_func));

    print("printing new func:");
    new_func->print(outs());
    print("checking:");
    
    if (verifyFunction(*new_func, &outs())) {
      cout << "Defined Function failed verification: " << endl;
      exit(1);
    }
  }

};

struct ArgsBuilder : NodeBuilder {
  std::vector<llvm::Type*> types;
  std::vector<string> names;
  
  ArgsBuilder() {
    register_callback("vardef", std::bind(&ArgsBuilder::p_vardef, this, _1));
  }

  void p_vardef(int n) {
    SearchNode node{n, pg};
    
    auto type_node = node.child("type");
    TypeBuilder builder;
    type_node.bottom_up(builder);
    
    auto name = node.child("name").text();
    auto type = llvm_type(type_node.N);

    names.emplace_back(name);
    nodes.emplace_back(type);
  }
};

struct StructDefBuilder : NodeBuilder {
  void f() {
    /*StructDefinition struct_def;

    int name_n = pg.children(n)[0];
    int struct_n = pg.children(n)[1];
    struct_def.name = pg.text(name_n);
    auto process_entries = [this, &struct_def] (ParseGraph &pg, int n) -> bool {
      if (pg.type(n) == "vardef") {
        auto type_n = pg.children(n)[0];
        auto name_n = pg.children(n)[1];
        pg.visit_bottom_up(type_n, bind(&ModuleBuilder::process_type, this, _1, _2));
        string name = pg.text(name_n);
        struct_def.add_type(name, get<llvm::Type*>(value_vector[type_n]));
        // context.add_type(name, get<llvm::Type*>(value_vector[type_n]));
        return false;
      }
      return true;
    };
    pg.visit_bottom_up(struct_n, process_entries);
    struct_def.build_type();
  }*/
};
