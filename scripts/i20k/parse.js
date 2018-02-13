function(context, args)
{
	function debug()
	{
		print("hi")
	}
	
	print("Eyy");
	
	var result = #fs.i20k.test();
	
	result.debug();
		
	return {debug:debug}
}