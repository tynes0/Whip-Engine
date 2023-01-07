workspace "Whip"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{}"

project "Whip"
	location "Whip"
	kind "SharedLib"
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
		"Whip/vendor/spdlog/include"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"WH_PLATFORM_WINDOWS",
			"WH_BUILD_DLL"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/F-Box")
		}

	filter "configurations:Debug"
		defines "WH_DEBUG"
		symbols "On"
		
	filter "configurations:Release"
		defines "WH_RELEASE"
		optimize "On"
		
	filter "configurations:Dist"
		defines "WH_DIST"
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
		"Whip"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"WH_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "WH_DEBUG"
		symbols "On"
		
	filter "configurations:Release"
		defines "WH_RELEASE"
		optimize "On"
		
	filter "configurations:Dist"
		defines "WH_DIST"
		optimize "On"
