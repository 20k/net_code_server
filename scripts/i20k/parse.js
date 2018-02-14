function(context, args)
{
	function debug()
	{
		print("hi")
	}
	
	print("args " + args);
	
	print("Eyy");
	
	///hmm. Interesting idea
	///detect #fs.i20k.test
	///convert to fs_i20k_test
	///then register a native function with that name
	///however... i dont'
	///that way we can have no dynamic functions
	var result = #fs.i20k.test({hi:"yes"});
	
	result.debug(1);
	
	var function_object = #fs.i20k.funcobject;
	
	function_object();
	function_object();
	
	function_object.call();
	
	//#D({test:"GLORIOUS_HASH_D"});
	
	//#D(context.caller);
	
	#db.r({});
	
	#db.i({name:"SCRIPT_NAME", doot:"doot"});
	#db.i({name2:"SCRIPT_NAME2"});
	
	#db.i({test:"hola"});
	#db.r({test:"hola"});
	
	var found = #db.f({test:"hola"}).array();
	
	//print("pf\n");
	//print(found);
	
	var res_cursor = #db.f({name:"SCRIPT_NAME"}, {doot:0});
	
	//return JSON.stringify(res_cursor);
	
	//
	return res_cursor.array();
	
	return context.caller;
}