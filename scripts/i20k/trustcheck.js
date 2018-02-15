function(context, args)
{
	/*var res = #fs.scripts.get_level({name:"accts.balance"});
	
	print(JSON.stringify(res));
	
	return #hs.accts.balance();*/
	
	return #ms.accts.xfer_gc_to({to:"test_user2", amount:1});
}