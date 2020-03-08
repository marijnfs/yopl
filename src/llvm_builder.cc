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
    value_vector[n] = builder->CreateLoad(value_ptr, false);
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
            auto struct_name = value_type->getPointerElementType()->getStructName();
            auto struct_member = child_node.text();
            auto index = context->get_struct_member_index(struct_name, struct_member);
            auto index_constant = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), index);
            value_ptr = builder->CreateGEP(value_ptr, std::vector<llvm::Value*>{index_constant});
        }
        if (child_node.type() == "number") {
            value_ptr = builder->CreateGEP(value_ptr, std::vector<llvm::Value*>{llvm_value(child_node.N)});
        }
        if (child_node.type() == "ptr") {
            value_ptr = builder->CreateLoad(value_ptr);
        }
    }

    std::cout << "setting valueptr " << n << " to " << value_ptr << std::endl;
    value_vector[n] = value_ptr;
}
