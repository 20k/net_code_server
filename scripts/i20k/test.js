function(context, args)
{
	function debug()
	{
		print("test debug")
	}
	
	print("testy");
		
	return {debug:debug}
}