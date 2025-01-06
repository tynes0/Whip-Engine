VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["stb_image"] = "%{wks.location}/Whip/vendor/stb_image"
IncludeDir["yaml_cpp"] = "%{wks.location}/Whip/vendor/yaml-cpp/include"
IncludeDir["Box2d"] = "%{wks.location}/Whip/vendor/box2d/include"
IncludeDir["GLFW"] = "%{wks.location}/Whip/vendor/GLFW/include"
IncludeDir["Glad"] = "%{wks.location}/Whip/vendor/Glad/include"
IncludeDir["ImGui"] = "%{wks.location}/Whip/vendor/ImGui"
IncludeDir["ImGuizmo"] = "%{wks.location}/Whip/vendor/ImGuizmo"
IncludeDir["filewatch"] = "%{wks.location}/Whip/vendor/filewatch"
IncludeDir["coco"] = "%{wks.location}/Whip/vendor/coco"
IncludeDir["nps"] = "%{wks.location}/Whip/vendor/nps"
IncludeDir["miniaudio"] = "%{wks.location}/Whip/vendor/miniaudio"
IncludeDir["glm"] = "%{wks.location}/Whip/vendor/glm"
IncludeDir["entt"] = "%{wks.location}/Whip/vendor/entt/include"
IncludeDir["msdfgen"] = "%{wks.location}/Whip/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdf_atlas_gen"] = "%{wks.location}/Whip/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["libogg"] = "%{wks.location}/Whip/vendor/libogg/include"
IncludeDir["Vorbis"] = "%{wks.location}/Whip/vendor/Vorbis/include"
IncludeDir["minimp3"] = "%{wks.location}/Whip/vendor/minimp3"
IncludeDir["OpenAL_Soft_include"] = "%{wks.location}/Whip/vendor/OpenAL-Soft/include"
IncludeDir["OpenAL_Soft_src"] = "%{wks.location}/Whip/vendor/OpenAL-Soft/src"
IncludeDir["OpenAL_Soft_src_common"] = "%{wks.location}/Whip/vendor/OpenAL-Soft/src/common"
IncludeDir["alhelpers"] = "%{wks.location}/Whip/vendor/alhelpers"
IncludeDir["mono"] = "%{wks.location}/Whip/vendor/mono/include"
IncludeDir["shaderc"] = "%{wks.location}/Whip/vendor/shaderc/include"
IncludeDir["SPIRV_Cross"] = "%{wks.location}/Whip/vendor/SPIRV-Cross"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["mono"] = "%{wks.location}/Whip/vendor/mono/lib/%{cfg.buildcfg}"

Library = {}
Library["mono"] = "%{LibraryDir.mono}/libmono-static-sgen.lib"
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{LibraryDir.VulkanSDK}/VkLayer_utils.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.VulkanSDK}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"
