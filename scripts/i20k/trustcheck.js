function(context, args)
{
	var res = #fs.scripts.get_level({name:"accts.balance"});
	
	print(JSON.stringify(res));
	
	return #hs.accts.balance();
}