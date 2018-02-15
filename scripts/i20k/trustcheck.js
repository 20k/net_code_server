function(context, args)
{
	/*var res = #fs.scripts.get_level({name:"accts.balance"});
	
	print(JSON.stringify(res));
	
	return #hs.accts.balance();*/
	
	var f2 = #ms.accts.xfer_gc_to({to:"test_user2", amount:1});
	
	print(f2);
	
	print(#fs.scripts.trust);
	
	//return #hs.accts.balance();
	
	
		
	//funcobject = funcobject.FUNC_ID;
	
	/*var ret = #ms.i20k.funcobject({f:funcobject});
	return ret;*/
	
	//return JSON.stringify(#fs.i20k.funcobject({f:funcobject}));
	
	//return JSON.stringify(ret);
	
	//return #hs.accts.xfer_gc_to({to:"test_user2", amount:1});
}