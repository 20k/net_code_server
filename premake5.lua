workspace "netcodeserver"
    architecture "x86_64"
    startproject "NetCodeServer"
    characterset "MBCS"

    configurations
    {
        "Debug",
        "Release",
    }

    flags
    {
        "MultiProcessorCompile",
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["include"] = "deps"
IncludeDir["ImGui"] = "deps/imgui"

ToolkitSourceFiles = {}
ToolkitSourceFiles["toolkit1"] = "deps/toolkit/clock.cpp"

NetworkingSourceFiles = {}
NetworkingSourceFiles["networking1"] = "deps/networking/beast_compilation_unit.cpp"
NetworkingSourceFiles["networking2"] = "deps/networking/networking.cpp"
NetworkingSourceFiles["networking3"] = "deps/networking/serialisable.cpp"

QuickjsSourceFiles = {}
QuickjsSourceFiles["qjs1"] = "deps/quickjs/cutils.c"
QuickjsSourceFiles["qjs2"] = "deps/quickjs/libbf.c"
QuickjsSourceFiles["qjs3"] = "deps/quickjs/libregexp.c"
QuickjsSourceFiles["qjs4"] = "deps/quickjs/libunicode.c"
QuickjsSourceFiles["qjs5"] = "deps/quickjs/quickjs.c"

project "NetCodeServer"
    location "."
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("builds/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("builds/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "*.hpp",
        "*.cpp",
        "deps/secret/**.cpp",
        "deps/secret/**.hpp",
        "%{ToolkitSourceFiles.toolkit1}",
        "%{NetworkingSourceFiles.networking1}",
        "%{NetworkingSourceFiles.networking2}",
        "%{NetworkingSourceFiles.networking3}",
        "%{QuickjsSourceFiles.qjs1}",
        "%{QuickjsSourceFiles.qjs2}",
        "%{QuickjsSourceFiles.qjs3}",
        "%{QuickjsSourceFiles.qjs4}",
        "%{QuickjsSourceFiles.qjs5}"
    }

    defines{
        "LOCAL_IP",
        "SYSTEM_TESTING",
        "EXTRAS",
        "BOOST_STACKTRACE_USE_BACKTRACE",
        "SERVER",
        "CONFIG_VERSION=\"\"",
        "CONFIG_BIGNUM",
        "DUMP_LEAKS",
        "USE_FIBERS",
    }

    includedirs
    {
        "./deps",
        "%{IncludeDir.include}",	
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.entt}",
        "/mingw64/include/freetype2"
    }

    libdirs
    {
        "deps/libs",
        "deps/steamworks_sdk_142/sdk/public/steam/lib/win64"
    }

    links
    {
        "mingw32",
        "sfml-system",
        "ws2_32",
        "boost_system-mt",
        "mswsock",
        "ole32",
        "boost_filesystem-mt",
        "dbgeng",
        "crypto",
        "ssl",
        "dl",
        "backtrace",
        "sdkencryptedappticket64",
        "boost_fiber-mt",
        "boost_context-mt",
        "lmdb"
    }

    linkoptions
    {
        "-fno-pie",
    }

    filter "system:windows"
        systemversion "latest"

        -- postbuildcommands -- copy dll after build
        -- {
        -- 	("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
        -- }

    buildoptions
    {
        "-std=c++17", "-Wall", "-Wextra", "-Wnon-virtual-dtor", "-Wunreachable-code", "-fexceptions", "-Wno-narrowing", 
        "-fno-strict-aliasing", "-Wno-unused-parameter", "-Wno-unused-label", "-no-pie", "-Werror=return-type", "-Wno-cast-function-type"    }

    configuration "Debug"
        defines {}
        runtime "Debug"
        symbols "on"
        
        buildoptions
        {
            "-Og", "-g"
        }

    configuration "Release"
        defines "ENGINE_RELEASE"
        runtime "Release"
        optimize "on"

        buildoptions 
        {
            "-O2"
        }