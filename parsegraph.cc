#include "parsegraph.h"

#include <fstream>
using namespace std;

void ParseGraph::print_dot(string filename) {
  ofstream dotfile(filename);

  dotfile << "digraph parsetree {" << endl;
  for (auto n(0); n < nodes.size(); ++n) {
    string name = name_map.count(name_ids[n]) ? name_map[name_ids[n]] : "";
    string sub =
        ends[n] >= 0 ? buffer.substr(starts[n], ends[n] - starts[n]) : "NEG";
    dotfile << "node" << n << " [label=\"" << name << " \'" << sub
            << "\' :" << starts[n] << "\"]" << endl;
    for (auto p : nodes[n].parents)
      dotfile << "node" << n << " -> "
              << "node" << p << endl;
  }
  dotfile << "}" << endl;
}

void ParseGraph::filter(function<void(ParseGraph &, int)> callback) {
  for (int i(0); i < nodes.size(); ++i)
    callback(*this, i);
}

void ParseGraph::compact() {
  // First create a node map, -1 for removing nodes
  int N(0);
  vector<int> node_map(nodes.size());
  for (int n(0); n < nodes.size(); ++n)
    if (cleanup[n])
      node_map[n] = -1;
    else
      node_map[n] = N++;

  // Make sure all removed nodes pass over their children and parent links
  for (int n(0); n < nodes.size(); ++n)
    if (cleanup[n]) {
      for (auto p : nodes[n].parents)
        nodes[p].children.insert(nodes[n].children.begin(),
                                 nodes[n].children.end());
      for (auto c : nodes[n].children)
        nodes[c].parents.insert(nodes[n].parents.begin(),
                                nodes[n].parents.end());
    }

  // Map over all indices, also for parents and children in all nodes
  for (int n(0); n < nodes.size(); ++n) {
    if (!cleanup[n]) {
      set<int> new_parents, new_children;
      for (auto p : nodes[n].parents)
        if (!cleanup[p])
          new_parents.insert(node_map[p]);
      for (auto c : nodes[n].children)
        if (!cleanup[c])
          new_children.insert(node_map[c]);
      nodes[n].parents = new_parents;
      nodes[n].children = new_children;

      nodes[node_map[n]] = nodes[n];
      starts[node_map[n]] = starts[n];
      ends[node_map[n]] = ends[n];
      name_ids[node_map[n]] = name_ids[n];
    }
  }

  // Resize everything
  nodes.resize(N);
  starts.resize(N);
  ends.resize(N);
  name_ids.resize(N);
  cleanup.resize(N);
  fill(cleanup.begin(), cleanup.end(), false);
}