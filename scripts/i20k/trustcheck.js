function(context, args)
{
	/*var res = #fs.scripts.get_level({name:"accts.balance"});
	
	print(JSON.stringify(res));
	
	return #hs.accts.balance();*/
	
	var f2 = #ms.accts.xfer_gc_to({to:"test_user2", amount:1});
	
	var funcobject = #ms.accts.xfer_gc_to;
	funcobject({to:"test_user2", amount:1});
	var ret = funcobject({to:"test_user2", amount:1});
	
	return #hs.accts.balance();
	
	//funcobject = funcobject.FUNC_ID;
	
	/*var ret = #ms.i20k.funcobject({f:funcobject});
	return ret;*/
	
	//return JSON.stringify(#fs.i20k.funcobject({f:funcobject}));
	
	//return JSON.stringify(ret);
	
	//return #hs.accts.xfer_gc_to({to:"test_user2", amount:1});
}