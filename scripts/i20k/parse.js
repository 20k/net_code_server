function(context, args)
{
	function debug()
	{
		print("hi")
	}
	
	print("Eyy");
	
	///hmm. Interesting idea
	///detect #fs.i20k.test
	///convert to fs_i20k_test
	///then register a native function with that name
	///that way we can have no dynamic functions
	///however... i don't exactly need to kill them, so
	var result = #fs.i20k.test();
	
	result.debug();
	
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
	
	//
	return res_cursor.array();
	
	return context.caller;
}