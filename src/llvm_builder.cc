#include "llvm_builder.h"

using namespace std;

void BlockBuilder::p_function(int n) {
  print("function irbuilder:", builder);
  SearchNode node{n, pg};
  FunctionBuilder function_builder(*this);
  node.visit(function_builder);
}
