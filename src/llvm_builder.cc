#include "llvm_builder.h"

using namespace std;

void BlockBuilder::p_function(int n) {
  print("function irbuilder:", builder);
  SearchNode node{n, pg};
  FunctionBuilder function_builder(*this);
  node.visit(function_builder);

  u_context->add_value(function_builder.name, function_builder.u_function.release());
}

void ExpBuilder::p_getelement(int n) {
   SearchNode node{n, pg};
    auto value_ptr = llvm_value(node.child().N);

    if (!value_ptr)
    	throw std::runtime_error("Failed to get llvm value");
    builder->CreateLoad(value_ptr, false);
}


void ExpBuilder::p_getelementptr(int n) {
    SearchNode node{n, pg};
    auto load_ptr = node.child(0);
    auto load_element = node.child(1);
    
    auto value_ptr = llvm_value(load_ptr.N);

    auto elements = load_element.get_all("element");
    for (auto n : elements) {
        auto value_type = value_ptr->getType();
        auto child_node = n.child();
        if (child_node.type() == "name") {
            if (value_type->isStructTy()) {
                auto name = value_type->getStructName();
                auto variable_index = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 0);
                value_ptr = builder->CreateGEP(value_ptr, std::vector<llvm::Value*>{variable_index});
                
            } else {
                throw std::runtime_error("can't find variable, base type is not a struct");
            }
        }
        if (child_node.type() == "number") {
            if (value_type->isPointerTy() || value_type->isArrayTy()) {
                value_ptr = builder->CreateGEP(value_ptr, std::vector<llvm::Value*>{llvm_value(child_node.N)});
            } else {
                throw std::runtime_error("Variable not a ptr or array type");
            }
        }
    }

    value_vector[node.N] = value_ptr;
}