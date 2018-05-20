#include "ruleset.h"
#include "const.h"
#include "parser.h"

#include <exception>
#include <fstream>
#include <sstream>

using namespace std;

RuleSet::RuleSet() {}

// setup ruleset through yopl itself
void RuleSet::yopl_load(string filename, LoadType load_type) {
  // base rules
  add_option("ROOT", vector<int>{2});
  add_end();

  string parser_file("test-files/gram.gram");
  LoadType grammar_load = LOAD_BASIC;
  if (load_type == LOAD_YOPLYOPL)
    grammar_load = LOAD_YOPL;
  Parser parser(parser_file, grammar_load);

  auto pg = parser.parse(filename);
  if (!pg)
    throw "failed to open file";

  int root = pg->root();
  cout << "root: " << root << endl;
  auto lines = pg->get_connected(root, "line", "S");
  cout << "nlines: " << lines.size() << endl;

  map<string, int> rule_option_map;
  for (int n : lines) {
    int rn = pg->get_one(n, "rulename");
    string rulename = pg->substr(rn);
    cout << "rule name: " << rulename << endl;
    int pos = add_option(rulename, vector<int>()); // fill rules in later
    rule_option_map[rulename] = pos;
    add_ret();
  }

  for (int l : lines) {
    int rn = pg->get_one(l, "rulename");
    string rulename = pg->substr(rn);
    for (int o : pg->get_connected(l, "option")) { // all options in this line
      bool first(true);

      for (int r : pg->get_connected(o, "rule")) { // go through all rule
        int rule_pos(-1); // rulepos will point to the added rule, so we can add
                          // it to the rule option the first time

        // figure out what kind of rule it is, spawn or matching
        // then add it and point rule_pos to it
        int n = pg->get_one(r, "name");
        if (n >= 0) { // rule is an option spawn
          string spawn_name = pg->substr(n);
          if (!rule_option_map.count(spawn_name)) {
            cerr << "couldn't find option: " << spawn_name << endl;
            throw "";
          }
          rule_pos = add_option(vector<int>{rule_option_map[spawn_name]});
        } else { // must be match name, either notquote or notdquote
          n = pg->get_one(r, "notquote");
          if (n < 0)
            n = pg->get_one(r, "notdquote");
          string match_str = pg->substr(n);
          rule_pos = add_match(match_str);
        }

        if (first) { // add the first rule to the main option arguments
          arguments[rule_option_map[rulename]].push_back(rule_pos);
          first = false;
        }
      }
      add_ret();
    }
  }

  // print rules
  if (PRINT_RULES) {
    for (int i(0); i < types.size(); ++i) {
      cout << i << ": [" << types[i] << "] ";
      cout << names[i] << " :";
      if (types[i] == OPTION)
        for (auto i : arguments[i])
          cout << i << ",";
      if (types[i] == MATCH)
        cout << "'" << matcher[i]->pattern() << "'";

      cout << endl;
    }
  }
}

RuleSet::RuleSet(string filename, LoadType load_type) {
  load(filename, load_type);
}

void RuleSet::load(string filename, LoadType load_type) {
  if (load_type == LOAD_YOPLYOPL)
    yopl_load(filename, load_type);
  if (load_type == LOAD_YOPL)
    yopl_load(filename, load_type);
  if (load_type == LOAD_BASIC)
    basic_load(filename);
}

void RuleSet::basic_load(string filename) {
  ifstream infile(filename.c_str());

  enum Mode { BLANK = 0, READ = 1, ESCAPESINGLE = 2, ESCAPEDOUBLE = 3 };

  string root_rule_name;

  map<string, vector<vector<string>>> rules;

  string line;
  while (getline(infile, line)) {
    istringstream iss(line);

    string name;
    iss >> name;
    if (name.size() == 0)
      continue;

    Mode mode(BLANK);
    string item;
    vector<string> curitems;
    vector<vector<string>> options;

    while (true) {
      char c = iss.get();
      if (c == EOF) {
        if (item.size())
          curitems.push_back(item);
        options.push_back(curitems);
        curitems.clear();
        if (!rules.size()) // this is the first rule, thus root rule
          root_rule_name = name;
        rules[name] = options;
        break;
      }

      if (mode == BLANK) {
        if (c == '|') { // a new set
          // add current items to vector
          options.push_back(curitems);
          curitems.clear();
          item.clear();
        } else if (c == ' ')
          ;
        else if (c == '\'')
          mode = ESCAPESINGLE;
        else if (c == '\"')
          mode = ESCAPEDOUBLE;
        else {
          item += c;
          mode = READ;
        }
      } else if (mode == READ) {
        if (c == ' ') {
          // add item to set
          curitems.push_back(item);
          item.clear();
          mode = BLANK;
        } else
          item += c;
      } else if (mode == ESCAPESINGLE) {
        if (c == '\'') {
          // add item to set
          curitems.push_back(item);
          item.clear();
          mode = BLANK;
        } else
          item += c;
      } else if (mode == ESCAPEDOUBLE) {
        if (c == '\"') {
          // add item to set
          curitems.push_back(item);
          item.clear();
          mode = BLANK;
        } else
          item += c;
      }
    }
  }

  // Add the rules
  add_option(
      "ROOT",
      vector<int>{-1}); // Spawn point will be updated after rules are made
  add_end();

  typedef pair<int, string> so_pair;
  multimap<int, string>
      search_option; // backsearching the option calls afterwards
  map<string, int> rule_pos;

  for (auto r : rules) { // go over rules, no particular order since std::map
    string name = r.first;
    vector<vector<string>> &options = r.second;

    int start = size();
    rule_pos[name] = start;

    if (options.size() == 1 &&
        options[0].size() == 1) { // we dont need an option
      auto &expressions = options[0];
      for (auto &exp : expressions) {
        if (rules.count(exp)) { // refers to a rule
          search_option.insert(so_pair(size(), exp));
          add_option();
        } else { // a matcher
          add_match(exp);
        }
      }
      add_ret();

    } else { // we need an option
      add_option();
      add_ret();

      for (auto o : options) {
        int op_start = size();
        if (o.size() == 1 && rules.count(o[0])) {
          search_option.insert(so_pair(start, o[0]));
        } else {
          arguments[start].push_back(size());
          for (auto exp : o) {
            if (rules.count(exp)) { // refers to a rule
              search_option.insert(so_pair(size(), exp));
              add_option();
            } else { // a matcher
              add_match(exp);
            }
          }

          add_ret();
        }
      }
    }
  }

  // Lets set the root spawn correctly
  arguments[0][0] = rule_pos[root_rule_name];

  // back reference option calls
  for (auto kv : search_option) {
    int option_pos = kv.first;
    string call_name = kv.second;
    arguments[option_pos].push_back(rule_pos[call_name]);
  }

  // set names
  for (auto kv : rule_pos)
    names[kv.second] = kv.first;

  // set returns
  int last_ret(0);
  for (int i(types.size() - 1); i > 0; --i) {
    if (types[i] == RETURN || types[i] == END)
      last_ret = i;
    returns[i] = last_ret;
  }

  // print rules
  if (PRINT_RULES) {
    for (int i(0); i < types.size(); ++i) {
      cout << i << ": [" << types[i] << "] ";
      cout << names[i] << " :";
      if (types[i] == OPTION)
        for (auto i : arguments[i])
          cout << i << ",";
      if (types[i] == MATCH)
        cout << "'" << matcher[i]->pattern() << "'";

      cout << endl;
    }
  }
}

int RuleSet::add_ret() {
  int pos = size();
  names.push_back("");
  types.push_back(RETURN);
  arguments.push_back(vector<int>(0));
  matcher.push_back(0);
  returns.push_back(-1);
  return pos;
}

int RuleSet::add_end() {
  int pos = size();
  names.push_back("");
  types.push_back(END);
  arguments.push_back(vector<int>(0));
  matcher.push_back(0);
  returns.push_back(-1);
  return pos;
}

int RuleSet::add_option(string name, vector<int> spawn) {
  int pos = size();
  names.push_back(name);
  types.push_back(OPTION);
  arguments.push_back(spawn); // call S
  matcher.push_back(0);
  returns.push_back(-1);
  return pos;
}

int RuleSet::add_option(vector<int> spawn) { return add_option("", spawn); }

int RuleSet::add_match(string name, string matchstr) {
  int pos = size();
  names.push_back(name);
  types.push_back(MATCH);
  arguments.push_back(vector<int>(0, 0));
  matcher.push_back(new RE2(matchstr));
  returns.push_back(-1);
  return pos;
}

int RuleSet::add_match(string matchstr) { return add_match("", matchstr); }

int RuleSet::size() { return types.size(); }
