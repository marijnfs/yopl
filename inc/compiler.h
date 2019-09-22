#pragma once

#include "yopl.h"
#include "yagll.h"

struct Compiler {
  LLVMContext C;
  
  unique_ptr<llvm::Module> module;
  unique_ptr<ParseGraph> parse_graph;

  // unique_ptr<BasicBlock> block;
  ValueVector value_vector;

  Compiler() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    
    module.reset(new llvm::Module(name, C));
  }
  
  void parse(ParseGraph &pg) {
    value_vector.resize(pg.size());
    //process_module(pg, 0);
  }

};
