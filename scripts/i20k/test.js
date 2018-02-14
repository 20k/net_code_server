function(context, args)
{
	function debug(targs)
	{
		print("test debug")
		
		if(targs)
		{
			print("targs");
		}
	}
	
	print("testy");
	
	if(args === undefined)
	{
		print("undef");
	}
	else
	{
		print(args.hi);
	}
	
	//print("hi args " + args.hi);
	print(args);
		
	return {debug:debug}
}