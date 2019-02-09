#include "../yagll/yagll.h"

#include <fstream>
#include <iostream>
#include <string>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;


using namespace std;
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

  cout << "nodes: " << parse_graph->size() << endl;
  parse_graph->filter([](ParseGraph &pg, int n) -> bool {
    if (pg.name(n) == "entryln")
      return true;
    if (pg.name(n) == "nocomment" && pg.substr(n) == "")
      return true;
    return false;        
  });

  parse_graph->remove([](ParseGraph &pg, int n) -> bool {
    if (pg.name(n) == "entry" && pg.get_one(n, "empty") != -1)
      return true;
    if (pg.name(n) == "optcom" && pg.substr(n).size() == 0)
      return true;
    return false;
  });

  parse_graph->squeeze([](ParseGraph &pg, int n) -> bool {
    auto name = pg.name(n);
    return name == "items" || name == "entries" || name == "clentries" || name == "nodes" || name == "flow" || name == "varname" || name == "sources";
  });

  ParseGraph::BoolCallback cb([](ParseGraph &pg, int n) -> bool {
    auto name = pg.name(n);
    if (name == "classdef") {
      cout << "found class: " << pg.substr(pg.children(n)[0]) << endl;
      return false;
    }
    return true;
  });
  parse_graph->visit_dfs(0, cb);

  parse_graph->print_dot("compact.dot");




  /*
  for (int n(0); n < parse_graph->nodes.size(); ++n) {
    if (parse_graph->name_ids[n] >= 0) {
      string sub = parse_graph->ends[n] < 0
                       ? "NEG"
                       : parser.buffer.substr(parse_graph->starts[n],
                                              parse_graph->ends[n] -
                                                  parse_graph->starts[n]);
      cout << parse_graph->name_map[parse_graph->name_ids[n]] << " " << sub
           << " " << parse_graph->starts[n] << endl;
    }
    }*/

  
}
