project "Whip"
	kind "StaticLib"
	staticruntime "off"
	language "C++"
	cppdialect "C++latest"

	targetdir ("%{wks.location}/bin/" ..outputdir.. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" ..outputdir.. "/%{prj.name}")

	pchheader "whippch.h"
	pchsource "src/whippch.cpp"


	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/coco/**.h",
		"vendor/nps/**.h",
		"vendor/frenum/**.h",
		"vendor/miniaudio/**.h",
		"vendor/miniaudio/**.cpp",
		"vendor/minimp3/**.h",
		"vendor/minimp3/**.cpp",
		"vendor/alhelpers/**.cpp",
		"vendor/alhelpers/**.h",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",
		"vendor/ImGuizmo/ImGuizmo.h",
		"vendor/ImGuizmo/ImGuizmo.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"IMGUI_DEFINE_MATH_OPERATORS",
		"GLFW_INCLUDE_NONE",
		"AL_LIBTYPE_STATIC"
	}


	includedirs
	{
		"src",
		"vendor/spdlog/include",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.Box2d}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.filewatch}",
		"%{IncludeDir.coco}",
		"%{IncludeDir.nps}",
		"%{IncludeDir.frenum}",
		"%{IncludeDir.miniaudio}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdf_atlas_gen}",
		"%{IncludeDir.libogg}",
		"%{IncludeDir.Vorbis}",
		"%{IncludeDir.minimp3}",
		"%{IncludeDir.OpenAL_Soft_include}",
		"%{IncludeDir.OpenAL_Soft_src}",
		"%{IncludeDir.OpenAL_Soft_src_common}",
		"%{IncludeDir.alhelpers}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.VulkanSDK}"
	}

	links
	{
		"Box2d",
		"GLFW",
		"Glad",
		"ImGui",
		"yaml-cpp",
		"opengl32.lib",
		"msdf-atlas-gen",
		"OpenAL-Soft",
		"Vorbis",
		"%{Library.mono}"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
		}

		links
		{
			"%{Library.WinSock}",
			"%{Library.WinMM}",
			"%{Library.WinVersion}",
			"%{Library.BCrypt}",
		}

	filter "configurations:Debug"
		defines "WHP_DEBUG"
		runtime "Debug"
		symbols "on"
		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}"
		}
		
	filter "configurations:Release"
		defines "WHP_RELEASE"
		runtime "Release"
		optimize "on"
		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}
		
	filter "configurations:Dist"
		defines "WHP_DIST"
		runtime "Release"
		optimize "on"
		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}
