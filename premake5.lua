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
IncludeDir["ImGui"] = "Whip/vendor/imgui"
IncludeDir["glm"] = "Whip/vendor/glm"

project "Whip"
	location "Whip"
	kind "StaticLib"
	staticruntime "on"
	language "C++"
	cppdialect "C++17"

	targetdir ("bin/" ..outputdir.. "/%{prj.name}")
	objdir ("bin-int/" ..outputdir.. "/%{prj.name}")

	pchheader "whippch.h"
	pchsource "Whip/src/whippch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}


	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"WHP_PLATFORM_WINDOWS",
			"WHP_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		buildoptions "/MDd"
		symbols "on"
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		buildoptions "/MD"
		optimize "on"
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		buildoptions "/MD"
		optimize "on"

project "F-Box"
	location "F-Box"
	kind "ConsoleApp"
	staticruntime "On"
	language "C++"
	cppdialect "C++17"

	targetdir ("bin/" ..outputdir.. "/%{prj.name}")
	objdir ("bin-int/" ..outputdir.. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"Whip/vendor/spdlog/include",
		"Whip/src",
		"Whip/vendor",
		"%{IncludeDir.glm}"
	}

	links
	{
		"Whip",
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"WHP_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		buildoptions "/MDd"
		symbols "on"
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		buildoptions "/MD"
		optimize "on"
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		buildoptions "/MD"
		optimize "on"


group "Additions"
	include "Whip/vendor/GLFW"
	include "Whip/vendor/Glad"
	include "Whip/vendor/ImGui"