workspace "Whip"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{}"

--include directories relative to root folder (sln directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Whip/vendor/GLFW/include"
IncludeDir["Glad"] = "Whip/vendor/Glad/include"

include "Whip/vendor/GLFW"
include "Whip/vendor/Glad"

project "Whip"
	location "Whip"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" ..outputdir.. "/%{prj.name}")
	objdir ("bin-int/" ..outputdir.. "/%{prj.name}")

	pchheader "whippch.h"
	pchsource "Whip/src/whippch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}


	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}"
	}

	links
	{
		"GLFW",
		"Glad",
		"opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"WHP_PLATFORM_WINDOWS",
			"WHP_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/F-Box")
		}

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		buildoptions "/MDd"
		symbols "On"
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		buildoptions "/MD"
		optimize "On"
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		buildoptions "/MD"
		optimize "On"

project "F-Box"
	location "F-Box"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/" ..outputdir.. "/%{prj.name}")
	objdir ("bin-int/" ..outputdir.. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}


	includedirs
	{
		"Whip/vendor/spdlog/include",
		"Whip/src"
	}

	links
	{
		"Whip",
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"WHP_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		buildoptions "/MDd"
		symbols "On"
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		buildoptions "/MD"
		optimize "On"
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		buildoptions "/MD"
		optimize "On"
