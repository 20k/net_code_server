function(context, args)
{
	function debug()
	{
		print("hi")
	}
	
	print("Eyy");
	
	var result = #fs.i20k.test();
	
	result.debug();
	
	//#D({test:"GLORIOUS_HASH_D"});
	
	//#D(context.caller);
	
	#db.i({name:"SCRIPT_NAME", doot:"doot"});
	#db.i({name2:"SCRIPT_NAME2"});
	
	#db.i({test:"hola"});
	#db.r({test:"hola"});
	
	var found = #db.f({test:"hola"}).array();
	
	//print("pf\n");
	//print(found);
	
	var res_cursor = #db.f({name:"SCRIPT_NAME"}, {doot:0});
	
	//
	return found;
	
	return context.caller;
}