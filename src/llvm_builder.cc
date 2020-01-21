#include "llvm_builder.h"

using namespace std;

void BlockBuilder::p_function(int n) {
  print("function irbuilder:", builder);
  SearchNode node{n, pg};
  FunctionBuilder function_builder(*this);
  node.visit(function_builder);

  u_context->add_value(function_builder.name, function_builder.u_function.release());
}

