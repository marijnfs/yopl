process_lines:
visit_dfs
register 'line' => bottom_up( process_exp )
register 'branch' => 
	bottom_up (bool val)
	if_block
	continue_block
	cond_br(get<Value>(bool val), if_block, continue_block)
	process_lines(if_block)
	b.setInsert(continues)


process_arguments:
	'vardef' => 
		child(0) > type	
		child(1).text > name
		bottom_up(type)
		get_type(type) > types.add
		name > names.add		

process_function:
	children > input, name, output, entries

	visit_dfs(input, process_argument) > names, types
	functiontype.get(double, types) > ftype
	func.create(ftype, externalLinkage, name.text, module) > f
	f.builder > builder
	context.new > context

	visit_dfs(entries, proces_lines, builder)
	verify(f)
	
	


process_exp:
visit_dfs
register 'number' => to_int(text) > constantFP() > n
register 'times' => b.createFDiv(child(0) child(1)) > n
register 'loadvarptr' => 
	text > var	
	context.get_val(var) > value
	if !value
		b.createAlloca(doublety, 0, var) > value
		context.add_val(var, value)
	value > n
register 'stat' => 
	(loaded value) child(0) > var_ptr
	b.createStore(var_ptr, child(1)) > n
register 'loadvar' =>
	child > text > var_name
	context.get_value(var_name) > var_ptr
	if !var_ptr > error
	b.createLoad(var_ptr) > n
register 'ret' =>
	b.createRet(child(0)) > n
register 'less' =>
	b.createFCmpUGT(get<value>(child 0), get<value>(child 1))
	

process_type:
	'i64' => getInt64TY > n
	'ptrof' => pointertype::get(type(child()) > n
	else => propagate

