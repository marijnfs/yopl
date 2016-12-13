#ifndef __YOPL_H__
#define __YOPL_H__

#include <vector>
#include <set>
#include <map>
#include <queue>
#include <re2/re2.h>
#include <string>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>



using namespace std;

struct NodeIndex {
  int cursor, rule;
  int nodeid;
  
  bool operator<(NodeIndex const &other) const {
    if (cursor != other.cursor)
      return cursor < other.cursor;
    return rule < other.rule;
  }

  bool operator>(NodeIndex const &other) const {
    if (cursor != other.cursor)
      return cursor > other.cursor;
    return rule > other.rule;
  }
};

ostream &operator<<(ostream &out, NodeIndex &ni) {
  return out << "c" << ni.cursor << " r" << ni.rule << " i" << ni.nodeid;
}


struct Head {
  int cursor, rule, depth;
  int node;
  
  bool operator<(Head const &other) const {
    if (cursor != other.cursor)
      return cursor < other.cursor;
    if (depth != other.depth)
      return depth < other.depth;
    return rule < other.rule;
  }

  bool operator>(Head const &other) const {
    if (cursor != other.cursor)
      return cursor > other.cursor;
    if (depth != other.depth)
      return depth > other.depth;
    return rule > other.rule;
  }
};

ostream &operator<<(ostream &out, Head &head) {
  return out << "c" << head.cursor << " r" << head.rule << " i" << head.node;
}


enum RuleType {
  OPTION = 0,
  PUSH = 1,
  MATCH = 2,
  RETURN = 3,
  END = 4
};

ostream &operator<<(ostream &out, RuleType &t) {
  switch (t) {
  case OPTION:
    return out << "OPTION"; break;
  case PUSH:
    return out << "PUSH"; break;
  case MATCH:
    return out << "MATCH"; break;
  case RETURN:
    return out << "RETURN"; break;
  case END:
    return out << "END"; break;
  }
}

struct RuleSet {
  std::vector<std::string> names;
  std::vector<RuleType> types;
  std::vector<std::vector<int>> arguments;
  std::vector<RE2*> matcher;

  //RuleSet(RuleSetDef rulesetdef);

  void add_ret() {
    types.push_back(RETURN);
    arguments.push_back(vector<int>(0,0));
    matcher.push_back(0);
  }

  void add_end() {
    types.push_back(END);
    arguments.push_back(vector<int>(0,0));
    matcher.push_back(0);
  }

  void add_option(vector<int> spawn) {
    types.push_back(OPTION);
    arguments.push_back(spawn); //call S
    matcher.push_back(0);
  }

  void add_match(RE2 *rule) {
    types.push_back(MATCH);
    arguments.push_back(vector<int>(0,0));
    matcher.push_back(rule);
  }
  
};

int match(RE2 &matcher, string &str, int pos = 0) {
  re2::StringPiece match;
  if (matcher.Match(str, pos, str.size(), RE2::ANCHOR_START, &match, 1)) {
    cout << "Matched " << match.length() << endl;
    return match.length();
  }
  //failed
  return -1;
}

int main(int argc, char **argv) {
  cout << "yopl" << endl;
  set<NodeIndex> stack;
  vector<NodeIndex> nodes;
  vector<set<int>> parents;
  vector<set<int>> ends;
  vector<int> properties;
  
  priority_queue<Head> heads;

  string buffer("aaaaa");
  RE2 rule("");

  
  RuleSet ruleset;
  ruleset.add_option(vector<int>{2});
  ruleset.add_end();
  ruleset.add_option(vector<int>{4,7});
  ruleset.add_ret();
  ruleset.add_option(vector<int>{2});
  ruleset.add_match(new RE2("a"));
  ruleset.add_ret();
  ruleset.add_match(new RE2(""));
  ruleset.add_ret();

  //add a node
  stack.insert(NodeIndex{0, 0, 0});
  nodes.push_back(NodeIndex{0, 0, 0});
  properties.push_back(0);
  parents.push_back(set<int>());
  ends.push_back(set<int>());

  //add first head
  heads.push(Head{0, 0, 0, 0});
  cout << heads.size() << endl;
  while(heads.size()) {
    for (auto p : parents)
      cout << p.size() << ' ';
    cout << endl;
    for (auto p : properties)
      cout << p << ' ';
    cout << endl;
    Head head = heads.top();
    cout << head << endl;
    cout << ruleset.types[head.rule] << endl;
    heads.pop();
    switch (ruleset.types[head.rule]) {
    case END:
      if (head.cursor == buffer.size()) {
	cout << "SUCCESS" << endl;
	return 0;
      }
      break;
    case RETURN:
      {
	int properties_node = properties[head.node];
	int cur = head.cursor;
	set<int> &par = parents[properties_node];
	cout << properties_node << " npar: " << par.size() << endl;
	cout << "what" << endl;
	int bla(0);
	for (int p : par) {
	  cout << "-" << par.size() << " " << parents.size();
	  ends[properties[p]].insert(cur);
	  //get rule of p
	  if (bla++ > 30) exit(1);
	  auto new_node = NodeIndex{cur, nodes[p].rule+1, nodes.size()};
	  if (stack.count(new_node))
	    ;//skip
	  else {
	    cout << "adding " << new_node << endl;
	    stack.insert(new_node);
	    nodes.push_back(new_node);
	    properties.push_back(properties[p]);
	    parents.push_back(set<int>());
	    ends.push_back(set<int>());
	    
	    heads.push(Head{new_node.cursor, new_node.rule, head.depth - 1, new_node.nodeid});
	  }
	}
      }
      break;
    case MATCH:
      {
	int n = head.node;
	int cur = head.cursor;
	int r = head.rule;
	int m = match(*ruleset.matcher[r], buffer, cur);
	if (m < 0) break; //no match

	//allright, add the node
	auto new_node = NodeIndex{cur + m, r+1, nodes.size()};

	cout << "adding " << new_node << endl;
	stack.insert(new_node);
	nodes.push_back(new_node);
	properties.push_back(properties[n]);
	parents.push_back(set<int>());
	ends.push_back(set<int>());

	heads.push(Head{new_node.cursor, new_node.rule, head.depth, new_node.nodeid});
      }
      break;
    case OPTION:
      {
	int n = head.node;
	int cur = head.cursor;
	int r = head.rule;
	vector<int> &args = ruleset.arguments[r];
	cout << args.size() << endl;
	for (int new_r : args) {
	  NodeIndex ni{cur, new_r, nodes.size()};
	  if (stack.count(ni)) {
	    int id = stack.find(ni)->nodeid;
	    //int n_ends = ends[id].size();
	    //if (!parents[id].count(n)) //should not be needed?
	    parents[id].insert(n);
	    for (int e : ends[id]) {
	      auto new_node = NodeIndex{e, r + 1, nodes.size()};
	      
	      cout << "adding " << new_node << endl;
	      stack.insert(new_node);
	      nodes.push_back(new_node);
	      properties.push_back(properties[n]);
	      parents.push_back(set<int>{});
	      ends.push_back(set<int>());
	      
	      heads.push(Head{new_node.cursor, new_node.rule, head.depth, new_node.nodeid});
	    }
	  } else {
	    //create node
	    auto new_node = ni;

	    cout << "adding " << new_node << endl;
	    stack.insert(new_node);
	    nodes.push_back(new_node);
	    properties.push_back(new_node.nodeid);
	    parents.push_back(set<int>{n});
	    ends.push_back(set<int>());

	    heads.push(Head{new_node.cursor, new_node.rule, head.depth + 1, new_node.nodeid});
	  }
	  
	}
      }
      break;
    }
  }
}

#endif
