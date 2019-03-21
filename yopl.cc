#include "../yagll/yagll.h"
#include "print.h"

#include <fstream>
#include <iostream>
#include <string>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;


using namespace std;

void tree_print(ParseGraph &pg, int n, int depth) {
  for (int i(0); i < depth; ++i)
    cout << "  ";
  if (pg.type(n) == "name" || pg.type(n) == "varname" || pg.type(n) == "value" || pg.type(n) == "number")
    cout << "[" << pg.starts[n] << "] " << pg.type(n) << "(" << pg.text(n) << ")" << endl;
  else
    cout << pg.type(n) << endl;

  for (auto c : pg.children(n))
    tree_print(pg, c, depth + 1); 
}

struct FunctionBuilder {

  FunctionBuilder() {

  }

  // std::vector<Type *> inputs;
  // FunctionType *FT = FunctionType::get(Type::getDoubleTy(C), inputs, false);
  // Function *bla =
  //     Function::Create(FT, Function::ExternalLinkage, "bla", mod);

  // bla->setCallingConv(CallingConv::C);

  // // auto args = bla->arg_begin();

  // BasicBlock *block = BasicBlock::Create(C, "entry", bla);

};

int main(int argc, char **argv) {
  cout << "yopl" << endl;
  if (argc < 3) {
    cerr << "not enough inputs, use: " << argv[0] << " [gram] [input]" << endl;
    return 1;
  }

  fs::path gram_path(argv[1]);
  fs::path input_path(argv[2]);

  if (!fs::exists(gram_path))
    throw StringException("Grammar file doesn't exist");
  if (!fs::exists(input_path))
    throw StringException("Input file doesn't exist");

  for (auto file : fs::directory_iterator(input_path.parent_path())) {
    cout << file << endl;
  }

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
     return name == "items" || name == "entries" || name == "clentries" || name == "nodes" || name == "flow" || name == "varname" || name == "sources";
    // return name == "items" || name == "entries" || name == "clentries" || name == "varname" || name == "sources";
  });


  parse_graph->sort_children([&parse_graph](int a, int b) -> bool {
    return parse_graph->starts[a] < parse_graph->starts[b];
  });

  //Actually process the info
  ParseGraph::BoolCallback cb([](ParseGraph &pg, int n) -> bool {
    auto name = pg.type(n);
    if (name == "classdef") {
      cout << "found class: " << pg.text(pg.children(n)[0]) << endl;
      return false;
    }
    return true;
  });

  parse_graph->visit_dfs(0, cb);
  parse_graph->print_dot("compact.dot");


  tree_print(*parse_graph, 0, 0);


  parse_graph->visit_dfs_filtered(0, [](ParseGraph &pg, int n) -> bool {
    if (pg.type(n) == "functiondef") {
        print(pg.type(n));
        print("func", pg.text(pg.children(n)[1]));
        pg.visit_dfs_filtered(n, [](ParseGraph &pg, int n) -> bool {
          if (pg.type(n) == "name")
            print(pg.text(n));
          return true;
        });
        return false;
    }

    return true;
  });
}
