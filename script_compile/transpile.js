var ts = require("typescript");
var fs = require("fs")

let nargs = process.argv.length

if(nargs < 3)
{
	console.log("Usage: \"source\"")
	return
}

var path = process.argv[2];

//console.log(path)

var text = fs.readFileSync(path,'utf8')

//console.log(text)

/*var parsed = process.argv[2];

var parsed2 = JSON.parse(parsed);

let result = ts.transpileModule(parsed, { compilerOptions: { module: ts.ModuleKind.CommonJS } });

console.log(result)*/

//var file_name = process.argv[2];

/*function compile(fileNames, options){
    let program = ts.createProgram(fileNames, options);
    let emitResult = program.emit();

    process.exit(0);
}

compile(process.argv.slice(2), {
    noEmitOnError: false, noImplicitAny: false,
    target: ts.ScriptTarget.ES5, module: ts.ModuleKind.CommonJS
});*/

//let result = ts.transpileModule(text, { compilerOptions: { module: ts.ModuleKind.CommonJS, noEmitOnError: false, noImplicitAny: false, downlevelIteration:true, target:ts.ScriptTarget.ES5 } });

//let r2 = result.outputText

var req = require("@babel/core")

var found = {};

try
{
    let after_type = ts.transpileModule(text, { compilerOptions: { module: ts.ModuleKind.CommonJS, noEmitOnError: false, noImplicitAny: false, downlevelIteration:true, target:ts.ScriptTarget.ES5 } });

    found["code_posttype"] = after_type;
    
    let r2 = req.transformSync(after_type.outputText, {sourceMaps:true, presets:[["@babel/preset-env"]]}, function(err, result) {
    });
    
    found["code_postbabel"] = r2;
}
catch(err)
{
    found["bable_error"] = err;
}

fs.writeFileSync(path + ".ts", JSON.stringify(found))