#include <yagll.h>
#include "print.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <memory>
#include <experimental/filesystem>

#include "llvm_builder.h"
#include "yopl.h"
#include "context.h"

using namespace std;
using namespace llvm;

int main(int argc, char **argv) {
  print("YOPL");
  if (argc < 3) {
    cerr << "not enough inputs, use: " << argv[0] << " [gram] [input]" << endl;
    return 1;
  }

  fs::path gram_path(argv[1]);
  fs::path input_path(argv[2]);

  if (!fs::exists(gram_path)) {
    println("Grammar file doesn't exist");
    return -1;
  }
  if (!fs::exists(input_path)) {

    println("Input file doesn't exist");
    return -1;
  }

  /*
  for (auto file : fs::directory_iterator(input_path.parent_path())) {
    cout << file << endl;
  }*/

  ifstream gram_file(gram_path);
  ifstream input_file(input_path);

  Parser parser(gram_file);
  auto parse_graph = parser.parse(input_file);
  
  if (!parse_graph)
    return 0;

  //filter empty lines and empty comments
  cout << "nodes: " << parse_graph->size() << endl;
  parse_graph->filter([](ParseGraph &pg, int n) -> bool {
                        if (pg.type(n) == "entryln")
                          return true;
                        if (pg.type(n) == "nocomment" && pg.text(n) == "")
                          return true;
                        return false;
                      });

  //remove empty stuff
  parse_graph->remove([](ParseGraph &pg, int n) -> bool {
                        if (pg.type(n) == "entry" && pg.get_one(n, "empty") != -1)
                          return true;
                        if (pg.type(n) == "optcom" && pg.text(n).size() == 0)
                          return true;
                        return false;
                      });

  //squeeze recursive multi-items
  parse_graph->squeeze([](ParseGraph &pg, int n) -> bool {
                         auto name = pg.type(n);
                         return name == "items" || name == "entries" || name == "structentries" || name == "nodes" || name == "flow" || name == "varname" || name == "sources";
                         // return name == "items" || name == "entries" || name == "structentries" || name == "varname" || name == "sources";
                       });
  
  parse_graph->sort_children([&parse_graph](int a, int b) -> bool {
                               return parse_graph->starts[a] < parse_graph->starts[b];
                             });
  
  parse_graph->print_dot("compact.dot");
  parse_graph->pprint(cout);

  ValueVector value_vector(parse_graph->size());
  LLVMContext C;
  
  NodeBuilder base_builder(*parse_graph, C, value_vector);
  
  ModuleBuilder module_builder(base_builder);
  print("Running Modulebuilder");
  parse_graph->visit(module_builder);
  module_builder.module->print(llvm::outs(), nullptr);
  print("Done");
}
