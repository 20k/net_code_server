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
	
	//function_object.call();
	
	print("post call\n");
	
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
	
	#db.u({name:"SCRIPT_NAME"}, {$set:{name:"update_works"}});
	
	var res_cursor = #db.f({name:"SCRIPT_NAME"}, {doot:0});
	
	#db.u({doot:{$exists:true}}, {$set:{doot:"nope"}});
	
	print("Is zero " + res_cursor.array().length);
	
	//return JSON.stringify(res_cursor);
	
	#ms.i20k.test();
	
	//
	//return res_cursor.array();
	
	print("find");
	
	print("testset\n");
	
	//return #db.f({name:"test"});
	
	var r2_curs = #db.f({name:"update_works"});
	
	print(r2_curs.array());
	
	return r2_curs.array();
	
	return context.caller;
}