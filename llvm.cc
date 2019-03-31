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

#include <iostream>

#include "../yagll/yagll.h"

using namespace std;
using namespace llvm;

Function *makeFunc(LLVMContext &C, Module *mod) {
  std::vector<Type *> inputs;
  inputs.push_back(Type::getInt32Ty(C));
  inputs.push_back(Type::getInt32Ty(C));
  inputs.push_back(Type::getInt32Ty(C));

  // Function *mul_add = mod->getFunction("mul_add");

  FunctionType *FT = FunctionType::get(Type::getInt32Ty(C), inputs, false);
  Function *mul_add =
      Function::Create(FT, Function::ExternalLinkage, "mul_add", mod);
  // Function *mul_add = cast<Function>(mod->getOrInsertFunction("muladd", FT));
  mul_add->setCallingConv(CallingConv::C);

  auto args = mul_add->arg_begin();

  Argument *x = &*args++;
  x->setName("x");
  Argument *y = &*args++;
  y->setName("y");
  Argument *z = &*args++;
  z->setName("z");

  BasicBlock *block = BasicBlock::Create(C, "entry", mul_add, 0);
  Value *tmp = BinaryOperator::CreateMul(x, y, "tmp", block);
  Value *result = BinaryOperator::CreateAdd(tmp, z, "result", block);
  ReturnInst::Create(C, result, block);


  // return mod;
  return mul_add;
}

void interpret(ParseGraph &pg) {
  cout << "n nodes:" << pg.size() << endl;
  vector<double> double_vec(pg.size());
  map<string, double> vars;

  auto callback = [&double_vec, &vars](ParseGraph &pg, int n) {
    auto rulename = pg.name(n);
    //cout << rulename << endl;
    if (rulename == "number") {
      double val(0);
      istringstream iss(pg.substr(n));
      iss >> val;
      double_vec[n] = val;
    } 
    else if (rulename == "times") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      double_vec[n] = double_vec[c1] * double_vec[c2];
    } 
    else if (rulename == "divide") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      double_vec[n] = double_vec[c1] / double_vec[c2];
    } 
    else if (rulename == "plus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      double_vec[n] = double_vec[c1] + double_vec[c2];
    } 
    else if (rulename == "minus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      double_vec[n] = double_vec[c1] - double_vec[c2];
    } 
    else if (rulename == "stat") {
      int namen = pg.get_one(n, "name");
      auto var_name = pg.substr(namen);
      auto var_val = double_vec[pg.children(n)[1]];
      //cout << "stat: " << var_name << " " << var_val << endl;
      vars[var_name] = var_val;
    } 
    else if (rulename == "var") {
      int namen = pg.get_one(n, "name");
      auto var_name = pg.substr(namen);
      if (!vars.count(var_name)) {
        cerr << "variable " << var_name << "doesnt exist" << endl;
      }
      double_vec[n] = vars[var_name];
    } 
    else if (rulename == "print") {
      cout << "p: " << double_vec[pg.children(n)[0]] << endl;
    } 
    else if (pg.children(n).size()) {
      double_vec[n] = double_vec[pg.children(n)[0]];
    }
  };
  pg.visit_bottom_up(0, callback);  


}

Function *make_func_yopl(LLVMContext &C, Module *mod, ParseGraph &pg) {
  std::vector<Type *> inputs;
  // Function *mul_add = mod->getFunction("mul_add");

  FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), inputs, false);
  Function *bla =
      Function::Create(FT, Function::ExternalLinkage, "bla", mod);

  bla->setCallingConv(CallingConv::C);

  // auto args = bla->arg_begin();

  BasicBlock *block = BasicBlock::Create(C, "entry", bla);

  typedef vector<Value*> value_vector;

  cout << "n nodes:" << pg.size() << endl;
  value_vector val_vec(pg.size());
  map<string, Value*> var_ptrs;

  for (auto var : pg.get_all(0, "svar")) {
    auto name = pg.substr(var);
    if (!var_ptrs.count(name)) {
      auto alloc = new AllocaInst(Type::getDoubleTy(C), 0, name, block);
      var_ptrs[name] = alloc;
    }
  }

  int ret_n(-1);

  auto callback = [&val_vec, &var_ptrs, &C, &ret_n, &block, &mod](ParseGraph &pg, int n) {
    auto rulename = pg.name(n);
    //cout << rulename << endl;
    if (rulename == "number") {
      double val(0);
      istringstream iss(pg.substr(n));
      iss >> val;
      val_vec[n] = ConstantFP::get(C, APFloat(val));
    } 
    else if (rulename == "times") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = BinaryOperator::CreateFMul(val_vec[c1], val_vec[c2], "", block);
    } 
    else if (rulename == "divide") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = BinaryOperator::CreateFDiv(val_vec[c1], val_vec[c2], "", block);
    } 
    else if (rulename == "plus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = BinaryOperator::CreateFAdd(val_vec[c1], val_vec[c2], "", block);
    }
    else if (rulename == "minus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = BinaryOperator::CreateFSub(val_vec[c1], val_vec[c2], "", block);

    } 
    else if (rulename == "stat") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      auto var_name = pg.substr(c1);
      val_vec[n] = new StoreInst(val_vec[c2], var_ptrs[var_name], block);
    } 
    else if (rulename == "var") {
      int c1 = pg.children(n)[0];
      auto var_name = pg.substr(c1);
      if (!var_ptrs.count(var_name)) {
        cerr << "no variable called " << var_name << endl;
        return;
      }
      val_vec[n] = new LoadInst(var_ptrs[var_name], 0, block);
    }
    else if (rulename == "func") {
      int cname = pg.children(n)[0];
      int cargs = pg.children(n)[1];
      int cfunc = pg.children(n)[2];
      
      std::vector<Type *> inputs;
      for (auto n : pg.get_all(cargs, "arg")) {
        inputs.push_back(Type::getDoubleTy(C));
      }
      FunctionType *func_type = FunctionType::get(Type::getDoubleTy(C), inputs, false);
      Function *function = Function::Create(func_type, Function::ExternalLinkage, "", mod);
      auto args = function->arg_begin();
      for (auto n : pg.get_all(cargs, "arg")) {
        args->setName(pg.substr(n));
        args++;
      }
      function->setCallingConv(CallingConv::C);

      cout << "func :" << pg.substr(cname) << " arg:" << pg.substr(cargs) << endl;
    }
    else if (rulename == "print") {
    }
    else if (rulename == "return") {
      int c1 = pg.children(n)[0];
      ret_n = c1;
      ReturnInst::Create(C, val_vec[c1], block);
    } 
    else if (pg.children(n).size()) {
      val_vec[n] = val_vec[pg.children(n)[0]];
    }
  };

  for (auto n : pg.get_all(0, "entry")) {
    pg.visit_bottom_up(n, callback);
  }

  if (ret_n < 0)
    ReturnInst::Create(C, ConstantFP::get(C, APFloat(0.0)), block);
  return bla;
//  for (auto t : test_vec)
 //   cout << t << " ";
}

int main(int argc, char **argv) {
  cout << "YOPL compiler" << endl;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  LLVMContext C;
  std::unique_ptr<Module> module(new Module("test", C));

  //Create the parser
  cout << "Contructing parser" << endl;
  ifstream gram_in("test-files/function.gram");
  ifstream input_in("test-files/function.input");
  Parser parser(gram_in);

  cout << "Parsing file" << endl;
  auto parse_graph = parser.parse(input_in);
  
  //interpret(*parse_graph.get());
  cout << "Compiling" << endl;
  Function *bla_func = make_func_yopl(C, module.get(), *parse_graph.get());

  //WriteBitcodeToFile(module.get(), outs());
  // bla_func = mod->getFunction("bla_func");
  std::string errStr;
  if (verifyFunction(*bla_func, &outs())) {
    cout << "failed verification" << endl;
    return 1;
  }
  if (verifyModule(*module, &outs())) {
    cout << "failed verification" << endl;
    return 1;
  }
  // if (verifyModule(*module)) {
  //   errs() << argv[0] << ": Error constructing function!\n";
  //   return 1;
  // }


  ExecutionEngine *EE =
      EngineBuilder(std::move(module)).setErrorStr(&errStr).create();


  if (!EE) {
    errs() << argv[0] << ": Failed to construct ExecutionEngine: " << errStr
           << "\n";
    return 1;
  }


  EE->finalizeObject();

  /*std::vector<GenericValue> Args(3);
  Args[0].IntVal = APInt(32, 1300);
  Args[1].IntVal = APInt(32, 1500);
  Args[2].IntVal = APInt(32, 1600);
  //GenericValue GV = EE->runFunction(muladd, Args);*/

  // cout << muladd << endl;
  typedef double bla();
  bla *f = (bla *)EE->getFunctionAddress("bla");
  double result = f();

  cout << "Result: " << result << "\n";

  return 0;
}
