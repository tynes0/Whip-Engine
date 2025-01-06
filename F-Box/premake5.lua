project "F-Box"
	kind "ConsoleApp"
	staticruntime "off"
	language "C++"
	cppdialect "C++20"

	targetdir ("%{wks.location}/bin/" ..outputdir.. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" ..outputdir.. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	includedirs
	{
		"%{wks.location}/Whip/vendor/spdlog/include",
		"%{wks.location}/Whip/src",
		"%{wks.location}/Whip/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}"
	}

	links
	{
		"Whip"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		runtime "Release"
		optimize "on"
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		runtime "Release"
		optimize "on"
