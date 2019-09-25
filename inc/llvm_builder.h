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
#include <variant>
#include <yagll.h>
#include "context.h"

using namespace std::placeholders;  // for _1, _2, _3...                        


typedef std::vector<std::variant<llvm::Value*, llvm::Type*>> ValueVector;
typedef std::map<std::string, llvm::Value*> ValueMap;

struct NodeBuilder : TypeCallback {
  llvm::LLVMContext &C;
  ValueVector &value_vector;

  std::shared_ptr<Context> context;
  std::shared_ptr<llvm::Function> current_func; 
  std::shared_ptr<llvm::IRBuilder<>> builder;
  std::shared_ptr<llvm::Module> module; 

  NodeBuilder(ParseGraph &pg_, llvm::LLVMContext &C_, ValueVector &value_vector_, TypeCallback::Mode mode_ = Mode::NONE, std::shared_ptr<llvm::Module> module_= 0, std::shared_ptr<Context> context_ = 0, std::shared_ptr<llvm::IRBuilder<>> builder_ = 0, std::shared_ptr<llvm::Function> current_func_ = 0) : 
    TypeCallback(&pg_, mode_),
    C(C_), 
    value_vector(value_vector_), 
    context(context_), 
    builder(builder_),
    current_func(current_func_),
    module(module_)
    {}

  NodeBuilder(NodeBuilder const &ohter) = default;
  NodeBuilder(NodeBuilder const &other, TypeCallback::Mode mode_) : NodeBuilder(other) {
    mode = mode_;
  }

  llvm::Value* llvm_value(int n) {
    return get<llvm::Value*>(value_vector[n]);
  }

  llvm::Type* llvm_type(int n) {
    return get<llvm::Type*>(value_vector[n]);
  }
};


struct ExpBuilder : NodeBuilder {
  ExpBuilder(NodeBuilder &other) :
    NodeBuilder(other, Mode::BOTTOM_UP) 
  {
    register_callback("number", std::bind(&ExpBuilder::p_number, this, _1));
    register_callback("times", std::bind(&ExpBuilder::p_times, this, _1));
    register_callback("divide", std::bind(&ExpBuilder::p_divide, this, _1));
    register_callback("plus", std::bind(&ExpBuilder::p_add, this, _1));
    register_callback("minus", std::bind(&ExpBuilder::p_subtract, this, _1));
    register_callback("nes", std::bind(&ExpBuilder::p_neg, this, _1));
    register_callback("loadvarptrcreate", std::bind(&ExpBuilder::p_loadvarptrcreate, this, _1));
    register_callback("loadvarptr", std::bind(&ExpBuilder::p_loadvarptr, this, _1));
    register_callback("loadvar", std::bind(&ExpBuilder::p_loadvar, this, _1));
    register_callback("stat", std::bind(&ExpBuilder::p_statement, this, _1));
    register_callback("ret", std::bind(&ExpBuilder::p_return, this, _1));
    register_callback("inc", std::bind(&ExpBuilder::p_inc, this, _1));
    register_callback("dev", std::bind(&ExpBuilder::p_dec, this, _1));
    register_callback("less", std::bind(&ExpBuilder::p_less, this, _1));
    register_callback("more", std::bind(&ExpBuilder::p_more, this, _1));
    register_callback("lesseq", std::bind(&ExpBuilder::p_less_eq, this, _1));
    register_callback("moreeq", std::bind(&ExpBuilder::p_more_eq, this, _1));
    register_callback("eq", std::bind(&ExpBuilder::p_eq, this, _1));
    register_callback("neq", std::bind(&ExpBuilder::p_neq, this, _1));
    register_callback("and", std::bind(&ExpBuilder::p_and, this, _1));
    register_callback("or", std::bind(&ExpBuilder::p_or, this, _1));
  }
  
  void p_number(int n) {
    SearchNode node{n, pg};
    
    value_vector[n] = llvm::ConstantFP::get(C, llvm::APFloat(node.text_to<double>()));
  }
  
  void p_times(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFMul(llvm_value(c0), llvm_value(c1));
  }
  
  void p_divide(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFDiv(llvm_value(c0), llvm_value(c1));
   }

  void p_add(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFAdd(llvm_value(c0), llvm_value(c1));
  }

  void p_subtract(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFSub(llvm_value(c0), llvm_value(c1));
  }

  void p_neg(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    value_vector[n] = builder->CreateFSub(llvm_value(c0), llvm::ConstantFP::get(C, llvm::APFloat(-1.0)));
  }

  void p_loadvarptrcreate(int n) {
    SearchNode node{n, pg};
    auto var_name = node.text();
    auto value_ptr = context->get_value(var_name);
    if (!value_ptr) {
      value_ptr = builder->CreateAlloca(llvm::Type::getDoubleTy(C), 0, var_name);
      context->add_value(var_name, value_ptr);
    }
    value_vector[n] = value_ptr;
  }

  void p_loadvarptr(int n) {
    SearchNode node{n, pg};
    auto var_name = node.text();
    auto value_ptr = context->get_value(var_name);
    if (!value_ptr) {
      print("couldn't locate ", var_name);
      throw std::runtime_error("undefined variable error");
    }
    value_vector[n] = value_ptr;
  }

  void p_loadvar(int n) {
    SearchNode node{n, pg};
    auto var_name = node.child().text();
    auto value_ptr = context->get_value(var_name);
    if (!value_ptr) {
      std::cerr << "no variable called " << var_name << std::endl;
      throw std::runtime_error("no var");
    }
    value_vector[n] = builder->CreateLoad(value_ptr, false);
  }
  
  void p_statement(int n) {
    SearchNode node{n, pg};
    int c0 = node.child(0).N;
    int c1 = node.child(1).N;
    auto target_ptr = llvm_value(c0);
    auto value = llvm_value(c1);
    value_vector[n] = builder->CreateStore(value, target_ptr);
  }
  
  void p_return(int n) {
    SearchNode node{n, pg};
    builder->CreateRet(llvm_value(node.child().N));
  }
  
  void p_inc(int n) {
    SearchNode node{n, pg};
    auto value_ptr = llvm_value(node.child(0).N);
    auto load = builder->CreateLoad(value_ptr, false);
    auto constant = llvm::ConstantFP::get(C, llvm::APFloat(1.0));
    auto add_inst = builder->CreateFAdd(load, constant);
    builder->CreateStore(add_inst, value_ptr);
    value_vector[n] = add_inst;
  }

  void p_dec(int n) {
    SearchNode node{n, pg};
    auto value_ptr = llvm_value(node.child(0).N);
    auto load = builder->CreateLoad(value_ptr, false);
    auto constant = llvm::ConstantFP::get(C, llvm::APFloat(1.0));
    auto sub_inst = builder->CreateFSub(load, constant);
    builder->CreateStore(sub_inst, value_ptr);
    value_vector[n] = sub_inst;
  }

  void p_less(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpULT(llvm_value(c0), llvm_value(c1));
  }

  void p_more(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpUGT(llvm_value(c0), llvm_value(c1));
  }

  void p_less_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpULE(llvm_value(c0), llvm_value(c1));
  }

  void p_more_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpUGE(llvm_value(c0), llvm_value(c1));
  }

  void p_eq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpUEQ(llvm_value(c0), llvm_value(c1));
  }

  void p_neq(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateFCmpUNE(llvm_value(c0), llvm_value(c1));
  }

  void p_and(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateAnd(llvm_value(c0), llvm_value(c1));
  }

  void p_or(int n) {
    SearchNode node{n, pg};
    auto c0 = node.child(0).N;
    auto c1 = node.child(1).N;
    value_vector[n] = builder->CreateOr(llvm_value(c0), llvm_value(c1));
  }

  void run_default(int n) {
    SearchNode node{n, pg};
    value_vector[n] = value_vector[node.child().N];
  }
};

struct BlockBuilder : NodeBuilder {
  BlockBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::TOP_DOWN) {
    register_callback("line", std::bind(&BlockBuilder::p_line, this, _1));
    register_callback("branch", std::bind(&BlockBuilder::p_branch, this, _1));
  }

  void p_line(int n) {
    SearchNode node{n, pg};
    ExpBuilder exp_builder(*this);
    node.visit(exp_builder);
  }

  void p_branch(int n) {
    auto new_context = std::unique_ptr<Context>(new Context(context.get()));
    SearchNode node{n, pg};

    auto condition = node.child("condition");
    auto body = node.child("body");
    
    ExpBuilder exp_builder(*this);
    condition.bottom_up(exp_builder);
    auto cond_val = get<llvm::Value*>(value_vector[condition.N]); //to search node?
    
    auto continued = llvm::BasicBlock::Create(C, "continued", current_func.get());
    auto if_block = llvm::BasicBlock::Create(C, "if", current_func.get());
    
    auto branchblock_node = body.child("branchblock").child();
    
    if (branchblock_node.type() == "if") {
      builder->CreateCondBr(cond_val, if_block, continued);
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder(*this);
      branchblock_node.child().visit(block_builder);
      
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      builder->SetInsertPoint(continued);
      
    } else if (branchblock_node.type() == "ifelse") {
      auto else_block = llvm::BasicBlock::Create(C, "else", current_func.get());
      builder->CreateCondBr(cond_val, if_block, else_block);
      
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder(*this);
      branchblock_node.child(0).visit(block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      
      builder->SetInsertPoint(else_block);
      BlockBuilder else_block_builder(*this);
      branchblock_node.child(1).visit(else_block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      
      builder->SetInsertPoint(continued);
    }
  }
};

struct TypeBuilder : NodeBuilder {
  TypeBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::BOTTOM_UP) {
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
    value_vector[n] = llvm::PointerType::get(llvm_type(node.child().N), 0);
  }
  
  void run_default(int n) {
    SearchNode node{n, pg};
    value_vector[n] = value_vector[node.child().N];
  }
};


struct ArgsBuilder : NodeBuilder {
  std::vector<llvm::Type*> types;
  std::vector<string> names;
  
  ArgsBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::TOP_DOWN) {
    register_callback("vardef", std::bind(&ArgsBuilder::p_vardef, this, _1));
  }

  void p_vardef(int n) {
    SearchNode node{n, pg};
    
    auto type_node = node.child("type");
    TypeBuilder builder(*this);
    type_node.bottom_up(builder);
    
    auto name = node.child("name").text();
    auto type = llvm_type(type_node.N);

    names.emplace_back(name);
    types.emplace_back(type);
  }
};

struct FunctionBuilder : NodeBuilder {
  FunctionBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::BOTTOM_UP) {
  }

  void p_function(int n) {
    SearchNode node{n, pg};

    ArgsBuilder args_builder(*this);
    node.child("in").bottom_up(args_builder);
    
    auto func_name = node.child("name").text();
    auto out = node.child("out");

    auto body = node.child("body");

    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(C), args_builder.types, false);

    llvm::Function *new_func =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, func_name, module.get());

    auto block = llvm::BasicBlock::Create(C, "entry", new_func);
    llvm::IRBuilder<> builder(block);
    auto func_context = std::make_shared<Context>(context.get());

    BlockBuilder block_builder(*this);
    body.top_down(block_builder);

    print("printing new func:");
    new_func->print(llvm::outs());
    print("checking:");
    
    if (verifyFunction(*new_func, &llvm::outs())) {
      print("Defined Function failed verification.");
      exit(1);
    }    
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
  }    
};

struct ModuleBuilder : NodeBuilder {
  std::unique_ptr<llvm::Module> module;

  ModuleBuilder(NodeBuilder &other) :
  NodeBuilder(other, Mode::TOP_DOWN)
  {
    module.reset(new llvm::Module("main", C));
  }

  void p_structdef(int n) {
  }
  
  void p_mainfunction(int n) {
    SearchNode module_node{n, pg};
    
    std::vector<llvm::Type *> inputs;
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(C), inputs, false);
    llvm::Function *main_func =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "main", module.get());
    main_func->setCallingConv(llvm::CallingConv::C);

    auto block = llvm::BasicBlock::Create(C, "entry", main_func);
    llvm::IRBuilder<> builder(block);

    BlockBuilder block_builder(*this);
    module_node.top_down(block_builder);

    //print created main func
    main_func->print(llvm::outs());
    if (verifyFunction(*main_func, &llvm::outs())) {
      print("Function failed verification: ");
      return;
    }

    if (verifyModule(*module, &llvm::outs())) {
      print("Module failed verification: ");
      return;      
    }

    //print whole module
    module->print(llvm::outs(), nullptr);

    //create the execution engine
    std::string errStr;
    auto EE =
        llvm::EngineBuilder(std::move(module)).setErrorStr(&errStr).create();

    if (!EE) {
      llvm::errs() << ": Failed to construct ExecutionEngine: " << errStr
             << "\n";
      return;
    }

    //compile    
    EE->finalizeObject();

    //get pointer to main func, and call it
    typedef double MainFunc();
    MainFunc *f = (MainFunc*) EE->getFunctionAddress("main");
    println("func: ptr", f);
    double result = f();
    println(result);
  }
};
