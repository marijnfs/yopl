- visit_bottom_up
	call callback from bottom up (visit dfs, add nodes to vector and reverse it. then call in order)
- visit_dfs_filtered
	call callback on all nodes, dfs. if callback returns false, don't continue that path (don't push)

- visit_bottom_up often has case switch on type
- visit_bottom_up also sometimes filteres for certain type, then returns false for callback

- visit_bottom_up also used with propagate

- specific remove function deletes unused parts (empty statements) (recursively)
- other squeeze removes while propagating children

- process lines: called with visit-dfs. has context argument, switches on types returning false (don't continue), calling visit-dfs or bottomup from there (possibly propagating context). 

- dfs is typically to process lines
- bottom_up is typically for evaluation, typically uses propagate

- perhaps use generators or callback structs to improve visiting mechanism
[generator]
generator would just for an iterable struct over which one can loop and do something with the nodes (either int or search nodes). Could take a vec<string> to match types, or perhaps vec<func > bool>.
[callback struct]
a struct that registers callbacks and then is run on a visit loop.
Could have register_type_callback, register_default_callback, register_func_callback etc.


process_module
process_exp
process_lines / process_block
process_function
process_type
process_structdef
process_arguments

