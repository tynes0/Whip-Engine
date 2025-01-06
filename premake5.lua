include "./vendor/premake/premake_customization/solution_items.lua"
include "dependencies.lua"

workspace "Whip"
	architecture "x86_64"
	startproject "F-Box"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"	
	}

	flags
	{
		"MultiProcessorCompile"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{}"

filter "files:Whip/vendor/ImGuizmo/**.cpp"
	flags { "NoPCH" }
filter {}
filter "files:Whip/vendor/alhelpers/**.cpp"
	flags { "NoPCH" }
filter {}

group "Additions"
	include "vendor/premake"
	include "Whip/vendor/box2d"
	include "Whip/vendor/GLFW"
	include "Whip/vendor/Glad"
	include "Whip/vendor/ImGui"
	include "Whip/vendor/yaml-cpp"
	include "Whip/vendor/msdf-atlas-gen"
	include "Whip/vendor/libogg"
	include "Whip/vendor/OpenAL-Soft"
	include "Whip/vendor/Vorbis"
group ""

group "Core"
	include "Whip"
	include "Whip-ScriptCore"
group ""

group "Tools"
	include "Whip-Editor"
group ""

group "Misc"
	include "F-Box"
group ""
