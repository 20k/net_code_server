function(context, args)
{
	function debug()
	{
		print("test debug")
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