function(context, args)
{
	function debug()
	{
		print("hi")
	}
	
	print("Eyy");
	
	var result = #fs.i20k.test();
	
	result.debug();
	
	#D({test:"GLORIOUS_HASH_D"});
	
	#D(context.caller);
	
	#db.i({name:"SCRIPT_NAME", doot:"doot"});
	#db.i({name2:"SCRIPT_NAME2"});
	
	#db.f({name:"SCRIPT_NAME"}, {doot:0});
	
	return context.caller;
}