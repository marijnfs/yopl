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
#include "llvm/IR/PassManager.h"
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

//predefs
struct NodeBuilder : TypeCallback {
  llvm::LLVMContext &C;
  ValueVector &value_vector;

  Context *context;
  llvm::Function *current_func = 0;
  llvm::IRBuilder<> *builder = 0;
  llvm::Module *module = 0;

  NodeBuilder(ParseGraph &pg_, llvm::LLVMContext &C_, ValueVector &value_vector_, TypeCallback::Mode mode_ = Mode::NONE, llvm::Module *module_= 0, Context *context_ = 0, llvm::IRBuilder<> *builder_ = 0, llvm::Function *current_func_ = 0) :
    TypeCallback(&pg_, mode_),
    C(C_), 
    value_vector(value_vector_), 
    context(context_),
    builder(builder_),
    current_func(current_func_),
    module(module_)
    {}

  NodeBuilder(NodeBuilder const &other) = default;
  NodeBuilder(NodeBuilder const &other, TypeCallback::Mode mode_) : NodeBuilder(other) { mode = mode_; }
  NodeBuilder(NodeBuilder const &other, TypeCallback::Mode mode_, Context *context_) : NodeBuilder(other, mode_) {
    context = context_;
  }

  llvm::Value* llvm_value(int n) {
    return std::get<llvm::Value*>(value_vector[n]);
  }

  llvm::Type* llvm_type(int n) {
    return std::get<llvm::Type*>(value_vector[n]);
  }

    void set_builder(llvm::IRBuilder<> *builder_) {
        builder = builder_;
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
    register_callback("call", std::bind(&ExpBuilder::p_call, this, _1));

    register_callback("continue", std::bind(&ExpBuilder::p_continue, this, _1));
    register_callback("break", std::bind(&ExpBuilder::p_break, this, _1));
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
      value_ptr = builder->CreateAlloca(llvm::Type::getDoubleTy(C), nullptr, var_name);
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

  void p_call(int n) {
    SearchNode node{n, pg};
    auto funcname = node.child("funcname").text();
    auto function = context->get_value(funcname);
    if (!function) {
        std::ostringstream oss;
        oss << "Function not defined: " << funcname;
        throw std::runtime_error(oss.str());
    }
    auto funcarg = node.child("funcarg");

    std::vector<llvm::Value*> argument_values;
    
    for (auto child : funcarg.get_all("value")) {
        print("Child: ", child.text());
        argument_values.emplace_back(llvm_value(child.N));
    }
    
    value_vector[n] = builder->CreateCall(function, argument_values);
  }

  void p_continue(int n) {
      print("adding continue");
      auto continue_block = context->get_continue();
      if (!continue_block) {
          throw std::runtime_error("No continue available at this point");
      }
      builder->CreateBr(continue_block);
  }

  void p_break(int n) {
      print("adding break");
      auto break_block = context->get_break();
      if (!break_block) {
          throw std::runtime_error("No break available at this point");
      }
      builder->CreateBr(break_block);
  }

  void run_default(int n) {
    //propagate
    
    SearchNode node{n, pg};
    auto child = node.child();
    if (child.valid()) //if there is a child, propagate it
      value_vector[n] = value_vector[node.child().N];
    else
      print("no child");
  }
};

struct BlockBuilder : NodeBuilder {
  std::unique_ptr<Context> u_context;

  BlockBuilder(NodeBuilder const &other, llvm::IRBuilder<> *builder_):
    BlockBuilder(other)
  {
    builder = builder_;
  }

  BlockBuilder(NodeBuilder const &other) 
    :  NodeBuilder(other, Mode::TOP_DOWN), 
       u_context(new Context(other.context)) 
  {
    context = u_context.get();
    register_callback("line", std::bind(&BlockBuilder::p_line, this, _1));
    register_callback("branch", std::bind(&BlockBuilder::p_branch, this, _1));
    register_callback("functiondef", std::bind(&BlockBuilder::p_function, this, _1));
  }

  void p_line(int n) {
    SearchNode node{n, pg};
    ExpBuilder exp_builder(*this);
    node.visit(exp_builder);
  }

  void p_function(int n);

  void p_branch(int n) {
    SearchNode node{n, pg};

    auto condition = node.child("condition");
    auto body = node.child("body");
    
    //generate evaluation of condition
    auto condition_evaluation = llvm::BasicBlock::Create(C, "condition", current_func);
    builder->CreateBr(condition_evaluation);
    builder->SetInsertPoint(condition_evaluation);
    
    ExpBuilder exp_builder(*this);
    exp_builder.set_builder(builder);
    condition.visit(exp_builder);

    auto cond_val = llvm_value(condition.N);

    /// create continued block, where excecution will continue after branch
    auto continued = llvm::BasicBlock::Create(C, "continued", current_func);
    
    /// create block for if condition is true
    auto if_block = llvm::BasicBlock::Create(C, "if", current_func);
    
    /// grab the sub node
    auto branchblock_node = body.child("branchblock").child();
    
    if (branchblock_node.type() == "if") {
      auto cond = builder->CreateCondBr(cond_val, if_block, continued);
      
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder(*this, builder);
      block_builder.context->break_block = continued; //set break block to 'continued' block that runs after branch
      block_builder.context->continue_block = condition_evaluation; //set continue block to if branch itself
      
      branchblock_node.child().visit(block_builder);
      
      ///Make sure there is a terminator. 
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      builder->SetInsertPoint(continued);
    } else if (branchblock_node.type() == "ifelse") {
        //create an else block
      auto else_block = llvm::BasicBlock::Create(C, "else", current_func);

      /// Create the conditional
      builder->CreateCondBr(cond_val, if_block, else_block);
      
      /// Build If Block
      builder->SetInsertPoint(if_block);
      BlockBuilder block_builder(*this, builder);
      block_builder.context->break_block = continued; //set break block to 'continued' block that runs after branch
      block_builder.context->continue_block = condition_evaluation; //set continue block 

      branchblock_node.child(0).visit(block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);

      // Build Else Block
      builder->SetInsertPoint(else_block);
      BlockBuilder else_block_builder(*this, builder);
      block_builder.context->break_block = continued; //set break block to 'continued' block that runs after branch
      block_builder.context->continue_block = condition_evaluation; //set continue block
      branchblock_node.child(1).visit(else_block_builder);
      if (builder->GetInsertBlock()->getTerminator() == nullptr)
        builder->CreateBr(continued);
      
      builder->SetInsertPoint(continued);
    }
  }
};

struct TypeBuilder : NodeBuilder {
  TypeBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::BOTTOM_UP) {
   register_callback("basetypename", std::bind(&TypeBuilder::p_basetypename, this, _1));
    register_callback("ptrof", std::bind(&TypeBuilder::p_ptrof, this, _1));
  }

  void p_basetypename(int n) {
    SearchNode node{n, pg};
    auto type_name = node.text();
    
    if (type_name == "i64")
      value_vector[n] = llvm::Type::getInt64Ty(C);
    else if (type_name == "i32")
      value_vector[n] = llvm::Type::getInt32Ty(C);
    else if (type_name == "i16")
      value_vector[n] = llvm::Type::getInt16Ty(C);
    else if (type_name == "f64")
      value_vector[n] = llvm::Type::getDoubleTy(C);
    else if (type_name == "f32")
      value_vector[n] = llvm::Type::getFloatTy(C);
    else
      throw std::runtime_error("Type not implemented");
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
    type_node.visit(builder);
    
    auto name = node.child("name").text();
    auto type = llvm_type(type_node.N);

    names.emplace_back(name);
    types.emplace_back(type);
  }

  int size() {
    return names.size();
  }
};

struct FunctionBuilder : NodeBuilder {
  std::unique_ptr<llvm::Function> u_function;
  std::string name;

  std::unique_ptr<Context> u_context;
  std::unique_ptr<llvm::IRBuilder<>> u_builder;

  FunctionBuilder(NodeBuilder &other) : NodeBuilder(other, Mode::BOTTOM_UP) {
    register_callback("functiondef", std::bind(&FunctionBuilder::p_function, this, _1));
  }

  void p_function(int n) {
    //prepare nodes
    SearchNode node{n, pg};
    auto out = node.child("out");
    auto body = node.child("body");

    //get fuction name
    name = node.child("name").text();
    print("Processing function: ", name);
    //create new context
    u_context.reset(new Context(context));
    context = u_context.get();
    
    //collect the input arguments
    ArgsBuilder args_builder(*this);
    node.child("in").visit(args_builder);

    //create the function type
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(C), args_builder.types, false);

    //create the function
    current_func =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, module);
    current_func->setCallingConv(llvm::CallingConv::C);

    //Set the unique ptr
    u_function.reset(current_func); 
    
    auto block = llvm::BasicBlock::Create(C, "entry", current_func);
    u_builder.reset(new llvm::IRBuilder<>(block));
    builder = u_builder.get();

    //grab the arguments, add them to the context
    {
      int n(0);
      for (auto &&arg : current_func->args()) {
        auto name = args_builder.names[n];
        arg.setName(name);
        //put argument on heap to make things easy, hope for llvmpasses to optimize it out
        auto storedArg = builder->CreateAlloca(arg.getType(), nullptr, name);
        builder->CreateStore(&arg, storedArg);
        context->add_value(name, storedArg);
        ++n;
      }
    }

    BlockBuilder block_builder(*this);
    body.visit(block_builder);
    
    if (verifyFunction(*current_func, &llvm::outs())) {
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
  std::unique_ptr<llvm::Module> u_module;
  //std::unique_ptr<llvm::Function> u_function; //not needed, module owns the function
  std::unique_ptr<Context> u_context;

  ~ModuleBuilder() {
    //if (u_module)
    //  u_function.release(); //if module is there, it will delete function on deletion, make sure we release it
  }


  ModuleBuilder(NodeBuilder &other) :
  NodeBuilder(other, Mode::TOP_DOWN)
  {
    register_callback("module", bind(&ModuleBuilder::p_mainfunction, this, _1));
    u_context.reset(new Context());
    context = u_context.get();
  }

  void p_structdef(int n) {
  }
  
  void p_mainfunction(int n) {
    std::string module_name = "Main";
    u_module.reset(new llvm::Module(module_name, C));
    module = u_module.get();

    SearchNode module_node{n, pg};
    
    std::vector<llvm::Type *> inputs;
    auto FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(C), inputs, false);
    current_func =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "modulemain", module);
    current_func->setCallingConv(llvm::CallingConv::C);
    //u_function.reset(current_func);

    //Create the function block and traverse the parsetree
    auto block = llvm::BasicBlock::Create(C, "entry", current_func);
    llvm::IRBuilder<> block_ir_builder(block);

    BlockBuilder block_builder(*this, &block_ir_builder);
    module_node.visit(block_builder);
    
    //print created main func
    if (verifyFunction(*current_func, &llvm::outs())) {
      print("Function failed verification: ");
      return;
    }

    if (verifyModule(*module, &llvm::outs())) {
      print("Module failed verification: ");
      return;      
    }

  }
};
