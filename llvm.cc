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

Function *make_func_yopl(LLVMContext &C, Module *mod) {
  ifstream gram_in("test-files/function.gram");
  ifstream input_in("test-files/function.input");
  Parser parser(gram_in);
  auto parse_graph = parser.parse(input_in);

  typedef vector<Value*> value_vector;

  cout << "n nodes:" << parse_graph->size() << endl;
  value_vector val_vec(parse_graph->size());
  vector<double> test_vec(parse_graph->size());

  map<string, double> vars;
  auto callback = [&val_vec, &test_vec, &vars, &C](ParseGraph &pg, int n) {
    auto rulename = pg.name(n);
    //cout << rulename << endl;
    if (rulename == "number") {
      double val(0);
      istringstream iss(pg.substr(n));
      iss >> val;
      val_vec[n] = ConstantInt::get(Type::getInt64Ty(C), val);
      test_vec[n] = val;
    } 
    else if (rulename == "times") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      val_vec[n] = BinaryOperator::CreateMul(val_vec[c1], val_vec[c2]);
      test_vec[n] = test_vec[c1] * test_vec[c2];
    } 
    else if (rulename == "divide") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      test_vec[n] = test_vec[c1] / test_vec[c2];
    } 
    else if (rulename == "plus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      test_vec[n] = test_vec[c1] + test_vec[c2];
    } 
    else if (rulename == "minus") {
      int c1 = pg.children(n)[0];
      int c2 = pg.children(n)[1];
      test_vec[n] = test_vec[c1] - test_vec[c2];
    } 
    else if (rulename == "stat") {
      int namen = pg.get_one(n, "name");
      auto var_name = pg.substr(namen);
      auto var_val = test_vec[pg.children(n)[1]];
      //cout << "stat: " << var_name << " " << var_val << endl;
      vars[var_name] = var_val;
    } 
    else if (rulename == "var") {
      int namen = pg.get_one(n, "name");
      auto var_name = pg.substr(namen);
      if (!vars.count(var_name)) {
        cerr << "variable " << var_name << "doesnt exist" << endl;
      }
      test_vec[n] = vars[var_name];
    } 
    else if (rulename == "print") {
      cout << "p: " << test_vec[pg.children(n)[0]] << endl;
    } 
    else if (pg.children(n).size()) {
      val_vec[n] = val_vec[pg.children(n)[0]];
      test_vec[n] = test_vec[pg.children(n)[0]];

    }
  };
  parse_graph->visit_bottom_up(0, callback);
//  for (auto t : test_vec)
 //   cout << t << " ";
}

int main(int argc, char **argv) {
  cout << "BEGIN" << endl;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  LLVMContext C;
  std::unique_ptr<Module> module(new Module("test", C));

  Function *muladd = make_func_yopl(C, module.get());

  // muladd = mod->getFunction("muladd");
  if (verifyFunction(*muladd)) {
    cout << "failed verification" << endl;
    return 1;
  }

  std::string errStr;
  ExecutionEngine *EE =
      EngineBuilder(std::move(module)).setErrorStr(&errStr).create();

  if (!EE) {
    errs() << argv[0] << ": Failed to construct ExecutionEngine: " << errStr
           << "\n";
    return 1;
  }

  errs() << "verifying... ";
  if (verifyModule(*module)) {
    errs() << argv[0] << ": Error constructing function!\n";
    return 1;
  }

  EE->finalizeObject();

  /*std::vector<GenericValue> Args(3);
  Args[0].IntVal = APInt(32, 1300);
  Args[1].IntVal = APInt(32, 1500);
  Args[2].IntVal = APInt(32, 1600);
  //GenericValue GV = EE->runFunction(muladd, Args);*/

  // cout << muladd << endl;
  typedef uint32_t bla(uint32_t, uint32_t, uint32_t);

  bla *f = (bla *)EE->getFunctionAddress("mul_add");

  outs() << "Result: " << f(1233, 12315, 61313) << "\n";

  return 0;
}
