function(context, args)
{
	/*var res = #fs.scripts.get_level({name:"accts.balance"});
	
	print(JSON.stringify(res));
	
	return #hs.accts.balance();*/
	
	var funcobject = #hs.accts.xfer_gc_to({to:"test_user2", amount:1});
	
	//var funcobject = #hs.accts.xfer_gc_to;
	
	//funcobject({to:"test_user2", amount:1});
	
	//funcobject = funcobject.FUNC_ID;
	
	return JSON.stringify(funcobject);
	
	//return #hs.accts.xfer_gc_to({to:"test_user2", amount:1});
}