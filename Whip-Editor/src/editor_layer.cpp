#include "editor_layer.h"

#include <Whip/Core/EntryPoint.h>
#include <Whip/Scene/scene_serializer.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/UI/UI_project_loader.h>
#include <Whip/Math/math.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/scene_importer.h>
#include <Whip/Asset/texture_atlas_parser.h>

#include "Helpers/icon_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <entt.hpp>
#include <ImGuizmo.h>
#include <yaml-cpp/yaml.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

_WHIP_START

namespace
{
	bool is_control_down()
	{
		return input::is_key_down(key::left_control) || input::is_key_down(key::right_control);
	}

	int gizmo_snap_index(int operation)
	{
		if (operation == ImGuizmo::OPERATION::TRANSLATE)
			return 0;
		if (operation == ImGuizmo::OPERATION::ROTATE)
			return 1;
		if (operation == ImGuizmo::OPERATION::SCALE || operation == ImGuizmo::OPERATION::SCALEU)
			return 2;
		return -1;
	}

	ImU32 color_u32(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	constexpr UI::editor_shortcut_action command_palette_actions[] =
	{
		UI::editor_shortcut_action::OpenProject,
		UI::editor_shortcut_action::NewScene,
		UI::editor_shortcut_action::SaveScene,
		UI::editor_shortcut_action::SaveSceneAs,
		UI::editor_shortcut_action::SaveProject,
		UI::editor_shortcut_action::CloseScene,
		UI::editor_shortcut_action::Undo,
		UI::editor_shortcut_action::Redo,
		UI::editor_shortcut_action::SelectAll,
		UI::editor_shortcut_action::Copy,
		UI::editor_shortcut_action::Paste,
		UI::editor_shortcut_action::Cut,
		UI::editor_shortcut_action::DuplicateEntity,
		UI::editor_shortcut_action::DeleteEntity,
		UI::editor_shortcut_action::Play,
		UI::editor_shortcut_action::Simulate,
		UI::editor_shortcut_action::Stop,
		UI::editor_shortcut_action::Pause,
		UI::editor_shortcut_action::GizmoNone,
		UI::editor_shortcut_action::GizmoTranslate,
		UI::editor_shortcut_action::GizmoRotate,
		UI::editor_shortcut_action::GizmoScale,
		UI::editor_shortcut_action::ReloadScripts,
		UI::editor_shortcut_action::OpenSettings,
		UI::editor_shortcut_action::OpenCommandPalette
	};

	std::string lower_copy(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	bool command_matches_filter(UI::editor_shortcut_action action, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		std::string needle = lower_copy(filter);
		std::string haystack = lower_copy(std::string(UI::UI_settings::get_action_display_name(action)) + " " + UI::UI_settings::get_action_category(action));
		return haystack.find(needle) != std::string::npos;
	}

	std::string sanitize_project_token(std::string value, const std::string& fallback)
	{
		std::erase_if(value, [](unsigned char c)
		{
			return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
		});

		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();

		if (value.empty())
			value = fallback;
		return value;
	}

	std::string sanitize_path_token(std::string value, const std::string& fallback)
	{
		value = sanitize_project_token(std::move(value), fallback);
		for (char& c : value)
			if (c == ' ')
				c = '_';
		return value;
	}

	void write_vec3(YAML::Emitter& out, const glm::vec3& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
	}

	glm::vec3 read_vec3(const YAML::Node& node, const glm::vec3& fallback)
	{
		if (!node || !node.IsSequence() || node.size() != 3)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z) };
	}

	UI::editor_theme theme_from_string(const std::string& value)
	{
		if (value == "Graphite")
			return UI::editor_theme::graphite;
		if (value == "Ember")
			return UI::editor_theme::ember;
		if (value == "Moss")
			return UI::editor_theme::moss;
		if (value == "Porcelain" || value == "Light")
			return UI::editor_theme::light;
		return UI::editor_theme::whip_dark;
	}

	std::string read_text_file(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return {};

		return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	}

	bool write_text_file(const std::filesystem::path& path, const std::string& contents)
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream << contents;
		return true;
	}

	std::filesystem::path normalize_project_list_path(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code error;
		std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, error);
		if (error)
		{
			error.clear();
			normalized_path = std::filesystem::absolute(path, error);
		}

		return error ? path : normalized_path.lexically_normal();
	}

	bool paths_match_for_recent_project(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		const std::filesystem::path normalized_left = normalize_project_list_path(left);
		const std::filesystem::path normalized_right = normalize_project_list_path(right);
		return !normalized_left.empty() && normalized_left == normalized_right;
	}

	bool path_is_or_is_under(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		const std::filesystem::path normalized_path = path.lexically_normal();
		const std::filesystem::path normalized_directory = directory.lexically_normal();
		if (normalized_path == normalized_directory)
			return true;

		auto path_it = normalized_path.begin();
		auto directory_it = normalized_directory.begin();
		for (; directory_it != normalized_directory.end(); ++directory_it, ++path_it)
		{
			if (path_it == normalized_path.end() || *path_it != *directory_it)
				return false;
		}

		return true;
	}

	bool create_directory_checked(const std::filesystem::path& path, std::string_view label)
	{
		std::error_code error;
		std::filesystem::create_directories(path, error);
		if (!error)
			return true;

		WHP_EDITOR_ERROR(std::string("[Whip Hub] Could not create ") + std::string(label) + ": " + path.string() + " (" + error.message() + ")");
		return false;
	}

	uint64_t fnv1a64(std::string_view value)
	{
		uint64_t hash = 14695981039346656037ull;
		for (char c : value)
		{
			hash ^= static_cast<unsigned char>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	std::string make_guid(std::string_view seed)
	{
		uint64_t first = fnv1a64(seed);
		std::string second_seed(seed);
		second_seed += ":whip";
		uint64_t second = fnv1a64(second_seed);

		std::array<uint8_t, 16> bytes{};
		for (int i = 0; i < 8; ++i)
			bytes[i] = static_cast<uint8_t>((first >> ((7 - i) * 8)) & 0xff);
		for (int i = 0; i < 8; ++i)
			bytes[i + 8] = static_cast<uint8_t>((second >> ((7 - i) * 8)) & 0xff);
		bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0f) | 0x40);
		bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3f) | 0x80);

		std::ostringstream out;
		out << std::uppercase << std::hex << std::setfill('0') << "{";
		for (int i = 0; i < 16; ++i)
		{
			if (i == 4 || i == 6 || i == 8 || i == 10)
				out << "-";
			out << std::setw(2) << static_cast<int>(bytes[i]);
		}
		out << "}";
		return out.str();
	}

	std::filesystem::path locate_script_core_source_directory()
	{
		std::error_code error;
		for (std::filesystem::path probe = std::filesystem::current_path(); !probe.empty(); probe = probe.parent_path())
		{
			const std::array<std::filesystem::path, 2> candidates =
			{
				probe / "Resources" / "Scripts" / "Whip-ScriptCore" / "Source",
				probe / "Whip-ScriptCore" / "Source"
			};

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_directory(candidate, error))
					return candidate;
			}

			if (probe == probe.parent_path())
				break;
		}

		return {};
	}

	std::filesystem::path locate_script_core_binary()
	{
		std::error_code error;
		for (std::filesystem::path probe = std::filesystem::current_path(); !probe.empty(); probe = probe.parent_path())
		{
			const std::array<std::filesystem::path, 5> candidates =
			{
				probe / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "Whip-ScriptCore.dll",
				probe / "bin" / "Debug-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "bin" / "Release-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll",
				probe / "bin" / "Dist-windows-x86_64" / "Whip-Editor" / "Resources" / "Scripts" / "Whip-ScriptCore.dll"
			};

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
					return candidate;
			}

			if (probe == probe.parent_path())
				break;
		}

		return {};
	}

	bool sync_script_core_binary(const std::filesystem::path& scripts_directory)
	{
		const std::filesystem::path source = locate_script_core_binary();
		if (source.empty())
		{
			WHP_EDITOR_WARN("[Script Build] Could not find Whip-ScriptCore.dll to sync into the script workspace.");
			return false;
		}

		const std::filesystem::path destination = scripts_directory / "Binaries" / "Whip-ScriptCore.dll";
		std::error_code error;
		std::filesystem::create_directories(destination.parent_path(), error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Script Build] Could not create script binaries directory: ") + error.message());
			return false;
		}

		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
		if (error)
		{
			WHP_EDITOR_WARN(std::string("[Script Build] Could not copy Whip-ScriptCore.dll: ") + error.message());
			return false;
		}

		const std::filesystem::path source_pdb = source.parent_path() / "Whip-ScriptCore.pdb";
		if (std::filesystem::exists(source_pdb, error))
		{
			error.clear();
			std::filesystem::copy_file(source_pdb, destination.parent_path() / "Whip-ScriptCore.pdb", std::filesystem::copy_options::overwrite_existing, error);
		}
		return true;
	}

	bool copy_directory_recursive(const std::filesystem::path& source, const std::filesystem::path& destination)
	{
		std::error_code error;
		if (!std::filesystem::exists(source, error))
			return false;

		std::filesystem::create_directories(destination, error);
		if (error)
			return false;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(source, error))
		{
			if (error)
				return false;

			const std::filesystem::path relative = std::filesystem::relative(entry.path(), source, error);
			if (error)
				return false;

			const std::filesystem::path target = destination / relative;
			if (entry.is_directory(error))
			{
				std::filesystem::create_directories(target, error);
			}
			else if (entry.is_regular_file(error))
			{
				std::filesystem::create_directories(target.parent_path(), error);
				if (!error)
					std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, error);
			}

			if (error)
				return false;
		}

		return true;
	}

	std::string script_core_csproj_contents(const std::string& core_guid)
	{
		WHP_UNUSED(core_guid);
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
			<< "  <PropertyGroup>\n"
			<< "    <TargetFramework>net472</TargetFramework>\n"
			<< "    <RootNamespace>Whip</RootNamespace>\n"
			<< "    <AssemblyName>Whip-ScriptCore</AssemblyName>\n"
			<< "    <OutputType>Library</OutputType>\n"
			<< "    <OutputPath>..\\Binaries\\</OutputPath>\n"
			<< "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
			<< "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n"
			<< "    <LangVersion>latest</LangVersion>\n"
			<< "    <GenerateAssemblyInfo>false</GenerateAssemblyInfo>\n"
			<< "  </PropertyGroup>\n"
			<< "</Project>\n";
		return stream.str();
	}

	std::string project_csproj_contents(const std::string& project_folder_name, const std::string& project_guid, const std::string& core_guid)
	{
		WHP_UNUSED(project_guid);
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
			<< "  <PropertyGroup>\n"
			<< "    <TargetFramework>net472</TargetFramework>\n"
			<< "    <RootNamespace>" << project_folder_name << "</RootNamespace>\n"
			<< "    <AssemblyName>" << project_folder_name << "</AssemblyName>\n"
			<< "    <OutputType>Library</OutputType>\n"
			<< "    <OutputPath>Binaries\\</OutputPath>\n"
			<< "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
			<< "    <LangVersion>latest</LangVersion>\n"
			<< "    <GenerateAssemblyInfo>false</GenerateAssemblyInfo>\n"
			<< "  </PropertyGroup>\n"
			<< "  <ItemGroup>\n"
			<< "    <Compile Remove=\"Whip-ScriptCore\\**\\*.cs\" />\n"
			<< "  </ItemGroup>\n"
			<< "  <ItemGroup>\n"
			<< "    <ProjectReference Include=\"Whip-ScriptCore\\Whip-ScriptCore.csproj\">\n"
			<< "      <Project>" << core_guid << "</Project>\n"
			<< "      <Name>Whip-ScriptCore</Name>\n"
			<< "      <Private>False</Private>\n"
			<< "    </ProjectReference>\n"
			<< "  </ItemGroup>\n"
			<< "</Project>\n";
		return stream.str();
	}

	std::string directory_build_props_contents(const std::string& project_folder_name)
	{
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project>\n"
			<< "  <PropertyGroup>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(TEMP)' != ''\">$(TEMP)/Whip/ScriptIntermediates/" << project_folder_name << "/</WhipScriptIntermediateRoot>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(WhipScriptIntermediateRoot)' == '' and '$(TMPDIR)' != ''\">$(TMPDIR)/Whip/ScriptIntermediates/" << project_folder_name << "/</WhipScriptIntermediateRoot>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(WhipScriptIntermediateRoot)' == ''\">$(MSBuildThisFileDirectory)Intermediates/" << project_folder_name << "/</WhipScriptIntermediateRoot>\n"
			<< "    <BaseIntermediateOutputPath>$(WhipScriptIntermediateRoot)$(MSBuildProjectName)/</BaseIntermediateOutputPath>\n"
			<< "    <MSBuildProjectExtensionsPath>$(BaseIntermediateOutputPath)</MSBuildProjectExtensionsPath>\n"
			<< "  </PropertyGroup>\n"
			<< "</Project>\n";
		return stream.str();
	}

	std::string script_solution_contents(const std::string& project_folder_name, const std::string& project_guid, const std::string& core_guid)
	{
		constexpr const char* csharp_project_type_guid = "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";
		std::ostringstream stream;
		stream
			<< "Microsoft Visual Studio Solution File, Format Version 12.00\n"
			<< "# Visual Studio Version 17\n"
			<< "VisualStudioVersion = 17.0.31903.59\n"
			<< "MinimumVisualStudioVersion = 10.0.40219.1\n"
			<< "Project(\"" << csharp_project_type_guid << "\") = \"" << project_folder_name << "\", \"" << project_folder_name << ".csproj\", \"" << project_guid << "\"\n"
			<< "EndProject\n"
			<< "Project(\"" << csharp_project_type_guid << "\") = \"Whip-ScriptCore\", \"Whip-ScriptCore\\Whip-ScriptCore.csproj\", \"" << core_guid << "\"\n"
			<< "EndProject\n"
			<< "Global\n"
			<< "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
			<< "\t\tDebug|x64 = Debug|x64\n"
			<< "\t\tRelease|x64 = Release|x64\n"
			<< "\t\tDist|x64 = Dist|x64\n"
			<< "\tEndGlobalSection\n"
			<< "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";

		const std::array<std::string, 3> configs = { "Debug|x64", "Release|x64", "Dist|x64" };
		for (const std::string& guid : { project_guid, core_guid })
		{
			for (const std::string& config : configs)
			{
				stream << "\t\t" << guid << "." << config << ".ActiveCfg = " << config << "\n";
				stream << "\t\t" << guid << "." << config << ".Build.0 = " << config << "\n";
			}
		}

		stream
			<< "\tEndGlobalSection\n"
			<< "\tGlobalSection(SolutionProperties) = preSolution\n"
			<< "\t\tHideSolutionNode = FALSE\n"
			<< "\tEndGlobalSection\n"
			<< "EndGlobal\n";
		return stream.str();
	}

	std::string starter_script_contents(const std::string& project_folder_name)
	{
		std::ostringstream stream;
		stream
			<< "using Whip;\n\n"
			<< "namespace " << project_folder_name << "\n"
			<< "{\n"
			<< "\tpublic class StarterEntity : Entity\n"
			<< "\t{\n"
			<< "\t\tpublic override void OnCreate()\n"
			<< "\t\t{\n"
			<< "\t\t\tLogger.Log(\"StarterEntity created.\", Logger.LogLevel.Info);\n"
			<< "\t\t}\n"
			<< "\t}\n"
			<< "}\n";
		return stream.str();
	}

	bool refresh_script_workspace_files(const std::filesystem::path& scripts_directory, const std::string& project_folder_name);

	bool write_script_project_files(const std::filesystem::path& project_directory, const std::string& project_folder_name)
	{
		const std::filesystem::path scripts_directory = project_directory / "Assets" / "Scripts";
		if (!refresh_script_workspace_files(scripts_directory, project_folder_name))
			return false;

		if (!write_text_file(scripts_directory / "Source" / "StarterEntity.cs", starter_script_contents(project_folder_name)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write starter script file.");
			return false;
		}

		sync_script_core_binary(scripts_directory);
		return true;
	}

	bool refresh_script_workspace_files(const std::filesystem::path& scripts_directory, const std::string& project_folder_name)
	{
		const std::string project_guid = make_guid(project_folder_name + ":scripts");
		const std::string core_guid = make_guid(project_folder_name + ":scriptcore");

		const std::filesystem::path script_core_source = locate_script_core_source_directory();
		if (script_core_source.empty() || !copy_directory_recursive(script_core_source, scripts_directory / "Whip-ScriptCore" / "Source"))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not copy Whip-ScriptCore SDK sources.");
			return false;
		}

		if (!write_text_file(scripts_directory / "Whip-ScriptCore" / "Whip-ScriptCore.csproj", script_core_csproj_contents(core_guid)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write Whip-ScriptCore project file.");
			return false;
		}

		if (!write_text_file(scripts_directory / "Directory.Build.props", directory_build_props_contents(project_folder_name)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write script build properties file.");
			return false;
		}

		if (!write_text_file(scripts_directory / (project_folder_name + ".csproj"), project_csproj_contents(project_folder_name, project_guid, core_guid)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write script project file.");
			return false;
		}

		if (!write_text_file(scripts_directory / (project_folder_name + ".sln"), script_solution_contents(project_folder_name, project_guid, core_guid)))
		{
			WHP_EDITOR_ERROR("[Whip Hub] Could not write script solution file.");
			return false;
		}

		return true;
	}

	std::filesystem::path find_script_project_file(const std::filesystem::path& scripts_directory, const std::string& project_name)
	{
		std::filesystem::path preferred = scripts_directory / (sanitize_path_token(project_name, "Untitled") + ".sln");
		std::error_code error;
		if (std::filesystem::exists(preferred, error))
			return preferred;

		preferred = scripts_directory / (sanitize_path_token(project_name, "Untitled") + ".csproj");
		if (std::filesystem::exists(preferred, error))
			return preferred;

		if (!std::filesystem::exists(scripts_directory, error))
			return {};

		for (const auto& entry : std::filesystem::directory_iterator(scripts_directory, error))
		{
			if (error)
				break;
			if (entry.is_regular_file(error) && entry.path().extension() == ".sln")
				return entry.path();
		}

		for (const auto& entry : std::filesystem::directory_iterator(scripts_directory, error))
		{
			if (error)
				break;
			if (entry.is_regular_file(error) && entry.path().extension() == ".csproj")
				return entry.path();
		}

		return {};
	}

	std::string quote_command_path(const std::filesystem::path& path)
	{
		return "\"" + path.string() + "\"";
	}

#ifdef _WIN32
	int run_windows_process(const std::string& command, bool log_output, std::string* first_line = nullptr)
	{
		SECURITY_ATTRIBUTES security_attributes{};
		security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		security_attributes.bInheritHandle = TRUE;

		HANDLE read_pipe = nullptr;
		HANDLE write_pipe = nullptr;
		if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0))
		{
			WHP_EDITOR_ERROR("[Script Build] Could not create process output pipe.");
			return -1;
		}
		SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA startup_info{};
		startup_info.cb = sizeof(STARTUPINFOA);
		startup_info.dwFlags = STARTF_USESTDHANDLES;
		startup_info.hStdOutput = write_pipe;
		startup_info.hStdError = write_pipe;
		startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION process_info{};
		std::string mutable_command = command;
		BOOL created = CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info);
		CloseHandle(write_pipe);

		if (!created)
		{
			const DWORD error = GetLastError();
			CloseHandle(read_pipe);
			WHP_EDITOR_ERROR(std::string("[Script Build] Could not start process. Windows error ") + std::to_string(error) + ".");
			return -1;
		}

		std::string pending;
		std::array<char, 512> buffer{};
		DWORD bytes_read = 0;
		while (ReadFile(read_pipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytes_read, nullptr) && bytes_read > 0)
		{
			buffer[bytes_read] = '\0';
			pending.append(buffer.data(), bytes_read);

			size_t newline = std::string::npos;
			while ((newline = pending.find('\n')) != std::string::npos)
			{
				std::string line = pending.substr(0, newline);
				pending.erase(0, newline + 1);
				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				if (first_line && first_line->empty() && !line.empty())
					*first_line = line;
				if (log_output && !line.empty())
					WHP_EDITOR_INFO(std::string("[Script Build] ") + line);
			}
		}

		if (!pending.empty())
		{
			if (!pending.empty() && pending.back() == '\r')
				pending.pop_back();
			if (first_line && first_line->empty() && !pending.empty())
				*first_line = pending;
			if (log_output && !pending.empty())
				WHP_EDITOR_INFO(std::string("[Script Build] ") + pending);
		}

		WaitForSingleObject(process_info.hProcess, INFINITE);
		DWORD exit_code = 0;
		GetExitCodeProcess(process_info.hProcess, &exit_code);

		CloseHandle(process_info.hProcess);
		CloseHandle(process_info.hThread);
		CloseHandle(read_pipe);
		return static_cast<int>(exit_code);
	}
#endif

	std::filesystem::path path_from_environment(const char* name)
	{
		const char* value = std::getenv(name);
		return value ? std::filesystem::path(value) : std::filesystem::path{};
	}

	std::filesystem::path find_executable_in_path(const std::string& executable_name)
	{
		const std::filesystem::path direct(executable_name);
		std::error_code error;
		if ((direct.is_absolute() || direct.has_parent_path()) && std::filesystem::exists(direct, error))
			return direct;

		std::vector<std::string> candidate_names = { executable_name };
#ifdef _WIN32
		if (std::filesystem::path(executable_name).extension().empty())
			candidate_names.push_back(executable_name + ".exe");
		constexpr char separator = ';';
#else
		constexpr char separator = ':';
#endif

		const char* path_env = std::getenv("PATH");
		if (!path_env)
			return {};

		std::stringstream stream(path_env);
		std::string directory;
		while (std::getline(stream, directory, separator))
		{
			if (directory.empty())
				continue;

			for (const std::string& candidate_name : candidate_names)
			{
				std::filesystem::path candidate = std::filesystem::path(directory) / candidate_name;
				error.clear();
				if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
					return candidate;
			}
		}

		return {};
	}

	std::string run_command_first_line(const std::string& command)
	{
#ifdef _WIN32
		std::string first_line;
		run_windows_process(command, false, &first_line);
		return first_line;
#else
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe)
			return {};

		std::array<char, 1024> buffer{};
		std::string line;
		if (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
			line = buffer.data();

		pclose(pipe);

		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();
		return line;
#endif
	}

	std::filesystem::path find_vswhere_executable()
	{
		if (std::filesystem::path path_candidate = find_executable_in_path("vswhere.exe"); !path_candidate.empty())
			return path_candidate;

		std::vector<std::filesystem::path> candidates;
		if (std::filesystem::path program_files_x86 = path_from_environment("ProgramFiles(x86)"); !program_files_x86.empty())
			candidates.push_back(program_files_x86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe");
		if (std::filesystem::path program_files = path_from_environment("ProgramFiles"); !program_files.empty())
			candidates.push_back(program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe");

		std::error_code error;
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
				return candidate;
		}

		return {};
	}

	std::filesystem::path find_msbuild_with_vswhere()
	{
		const std::filesystem::path vswhere = find_vswhere_executable();
		if (vswhere.empty())
			return {};

		const std::string command = quote_command_path(vswhere) + " -latest -products * -requires Microsoft.Component.MSBuild -find \"MSBuild\\**\\Bin\\MSBuild.exe\"";
		const std::string first_line = run_command_first_line(command);
		if (first_line.empty())
			return {};

		std::error_code error;
		std::filesystem::path candidate(first_line);
		if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
			return candidate;
		return {};
	}

	std::filesystem::path find_msbuild_executable()
	{
		if (std::filesystem::path env_msbuild = path_from_environment("WHIP_MSBUILD_PATH"); !env_msbuild.empty())
		{
			std::error_code error;
			if (std::filesystem::exists(env_msbuild, error))
				return env_msbuild;
			WHP_EDITOR_WARN(std::string("[Script Build] WHIP_MSBUILD_PATH is set but does not exist: ") + env_msbuild.string());
		}

		if (std::filesystem::path path_msbuild = find_executable_in_path("MSBuild.exe"); !path_msbuild.empty())
			return path_msbuild;

		if (std::filesystem::path vswhere_msbuild = find_msbuild_with_vswhere(); !vswhere_msbuild.empty())
			return vswhere_msbuild;

		const std::array<std::filesystem::path, 12> candidates =
		{
			"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/amd64/MSBuild.exe",
			"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe",
			"C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe"
		};

		std::error_code error;
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate, error))
				return candidate;
		}

		return {};
	}

	struct script_build_command
	{
		std::string command;
		std::string tool_name;
	};

	script_build_command make_script_build_command(const std::filesystem::path& build_file)
	{
		if (std::filesystem::path msbuild = find_msbuild_executable(); !msbuild.empty())
		{
			return {
				quote_command_path(msbuild) + " " + quote_command_path(build_file) + " /restore /nologo /v:minimal /p:Configuration=Debug /p:Platform=x64 /nr:false",
				msbuild.string()
			};
		}

		if (std::filesystem::path dotnet = find_executable_in_path("dotnet"); !dotnet.empty())
		{
			return {
				quote_command_path(dotnet) + " build " + quote_command_path(build_file) + " --nologo --restore -v:minimal -p:Configuration=Debug -p:Platform=x64",
				dotnet.string()
			};
		}

		return {};
	}

	int run_command_and_log_output(const std::string& command)
	{
#ifdef _WIN32
		return run_windows_process(command, true);
#else
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe)
		{
			WHP_EDITOR_ERROR("[Script Build] Could not start script build process.");
			return -1;
		}

		std::array<char, 512> buffer{};
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
		{
			std::string line(buffer.data());
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();
			if (!line.empty())
				WHP_EDITOR_INFO(std::string("[Script Build] ") + line);
		}

		return pclose(pipe);
#endif
	}
}

editor_layer::editor_layer()
	: layer("Fbox2D"), m_editor_camera()
{
	m_gizmo_type = ImGuizmo::OPERATION::TRANSLATE;
}

void editor_layer::on_attach()
{
    WHP_PROFILE_FUNCTION();

	m_animation_editor_panel.set_refresh_asset_tree_callback([this]() {if (m_content_browser_panel) { m_content_browser_panel->refresh_asset_tree(); } });
	m_scene_hierarchy_panel.set_scene_change_callback([this]() { capture_scene_history(); });
	m_UI_project.set_scene_callbacks(
		[this](asset_handle handle) { open_scene(handle); },
		[this]() { close_scene(); },
		[this]() { return m_editor_scene_path; });
	m_UI_project.set_before_change_callback([this]() { capture_scene_history(true); });
	m_UI_project.set_editor_settings_drawer([this]() { m_UI_settings.draw_content(); });
	setup_project_loader();
	load_editor_preferences();
	m_project_loader.set_recent_projects(m_recent_projects);

	// framebuffer
    framebuffer_specification fb_spec{};
    fb_spec.attachments = { framebuffer_texture_format::RGBA8, framebuffer_texture_format::RED_INTEGER, framebuffer_texture_format::depth };
    fb_spec.width = application::get().get_window().get_width();
    fb_spec.height = application::get().get_window().get_height();
    m_framebuffer = framebuffer::create(fb_spec);

	// scene
    m_editor_scene = make_ref<scene>();
	m_active_scene = m_editor_scene;

	// project
	auto command_line_args = application::get().get_specification().command_line_args;
	if (command_line_args.count > 1)
	{
		auto project_filepath = command_line_args[1];
		if (open_project(project_filepath))
			m_project_loader.set_loaded(true);
	}
	// camera
    m_editor_camera = editor_camera(30.0f, 1.778f, 0.1f, 1000.0f);
	console_panel::initialize();
	static float v1 = 0, v2 = 0;
	m_popup_handler
		.set_popup_name("Popup Testing")
		.set_height(300.f)
		.set_width(400.f)
		.add([]() { ImGui::Text("This is a text message for popup testing. Do not mind this window if you see that."); })
		.add([]() { static float fv = 0; ImGui::SliderFloat("##Float value", &fv, 0.0f, 10000.0f); })
		.same_line()
		.add([]() { static int iv = 0; ImGui::SliderInt("##Int value", &iv, 0, 1000000); })
		.add_dual_handle_slider(0, 100, &v1, &v2)
		.add_button([this]() { m_popup_handler.set_show_state(false); }, "Close", 100);

}

void editor_layer::on_detach()
{
	WHP_PROFILE_FUNCTION();
	save_editor_preferences();
	console_panel::shutdown();

	if (m_scene_state == scene_state::play)
		m_active_scene->on_runtime_stop();
	else if (m_scene_state == scene_state::simulate)
		m_active_scene->on_simulation_stop();

}

void editor_layer::on_update(timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_ts = ts;

	{
		WHP_PROFILE_SCOPE("Viewport Size");
		m_active_scene->on_viewport_resize(static_cast<uint32_t>(m_viewport_size.x), static_cast<uint32_t>(m_viewport_size.y));
		if (framebuffer_specification spec = m_framebuffer->get_specification();
			m_viewport_size.x > 0.0f &&
			m_viewport_size.y > 0.0f &&
			(spec.width != static_cast<uint32_t>(m_viewport_size.x) || spec.height != static_cast<uint32_t>(m_viewport_size.y)))
		{
			m_framebuffer->resize(static_cast<uint32_t>(m_viewport_size.x), static_cast<uint32_t>(m_viewport_size.y));
			m_editor_camera.set_viewport_size(m_viewport_size.x, m_viewport_size.y);
		}
	}

	{
		WHP_PROFILE_SCOPE("scene::on_update");
		renderer2D::reset_stats();
		m_framebuffer->bind();
		render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		render_command::clear();

		m_framebuffer->clear_attachment(1, -1);

		switch (m_scene_state)
		{
		case scene_state::edit:
		{
			if (!m_gizmo_using)
				m_editor_camera.on_update(ts);
			draw_editor_grid();
			m_active_scene->on_update_editor(ts, m_editor_camera);
			break;
		}
		case scene_state::play:
		{
			m_active_scene->on_update_runtime(ts);
			break;
		}
		case scene_state::simulate:
		{
			if (!m_gizmo_using)
				m_editor_camera.on_update(ts);
			draw_editor_grid();
			m_active_scene->on_update_simulation(ts, m_editor_camera);
			break;
		}
		}
	}

	{
		WHP_PROFILE_SCOPE("Mouse position track");
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_viewport_bounds[0].x;
		my -= m_viewport_bounds[0].y;
		glm::vec2 viewport_size = m_viewport_bounds[1] - m_viewport_bounds[0];
		my = viewport_size.y - my;
		int mouseX = static_cast<int>(mx);
		int mouseY = static_cast<int>(my);

		if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewport_size.x) && mouseY < static_cast<int>(viewport_size.y))
		{
			int pixel_data = m_framebuffer->read_pixel(1, mouseX, mouseY); // This is taking too much time
			m_hovered_entity = pixel_data == -1 ? entity() : entity(static_cast<entt::entity>(pixel_data), m_active_scene.get());
		}
	}

	on_overlay_render();

    m_framebuffer->unbind();
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
void editor_layer::on_imgui_render()
{
	WHP_PROFILE_FUNCTION();
	ImGuizmo::BeginFrame();
	m_gizmo_hovered = false;
	m_gizmo_using = false;
	const bool project_loaded = has_project_loaded();
	if (!project_loaded)
	{
		application::get().get_imgui_layer()->block_events(true);

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags hub_host_flags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		ImGui::Begin("Whip Hub Host", nullptr, hub_host_flags);
		m_project_loader.run();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		return;
	}

	// dockspace
	{
		static bool p_open = true;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor DockSpace", &p_open, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 300.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("Editor DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		style.WindowMinSize.x = minWinSizeX;
	}
	// menu bar
    if (ImGui::BeginMenuBar())
    {
		auto draw_menu_action = [this](UI::editor_shortcut_action action, const char* label = nullptr)
			{
				std::string shortcut = m_UI_settings.get_shortcut_label(action);
				const bool available = is_editor_action_available(action);
				ImGui::BeginDisabled(!available);
				bool clicked = ImGui::MenuItem(label ? label : UI::UI_settings::get_action_display_name(action), shortcut.c_str());
				ImGui::EndDisabled();
				if (clicked)
					execute_editor_action(action);
			};

        if (ImGui::BeginMenu("File"))
        {
			draw_menu_action(UI::editor_shortcut_action::OpenProject);
			draw_menu_action(UI::editor_shortcut_action::SaveProject);
			ImGui::Separator();
			draw_menu_action(UI::editor_shortcut_action::NewScene);
			draw_menu_action(UI::editor_shortcut_action::SaveScene);
			draw_menu_action(UI::editor_shortcut_action::SaveSceneAs, "Save Scene As...");
			draw_menu_action(UI::editor_shortcut_action::CloseScene);
			ImGui::Separator();
            if (ImGui::MenuItem("Restart"))
                application::get().submit_to_next_tick([]() { application::get().restart(); });
			if (ImGui::MenuItem("Exit"))
				application::get().close();
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Edit"))
		{
			draw_menu_action(UI::editor_shortcut_action::OpenCommandPalette);
			draw_menu_action(UI::editor_shortcut_action::OpenSettings, "Settings");
			ImGui::Separator();
			draw_menu_action(UI::editor_shortcut_action::Undo);
			draw_menu_action(UI::editor_shortcut_action::Redo);
			ImGui::Separator();
			draw_menu_action(UI::editor_shortcut_action::SelectAll);
			draw_menu_action(UI::editor_shortcut_action::Copy);
			draw_menu_action(UI::editor_shortcut_action::Paste);
			draw_menu_action(UI::editor_shortcut_action::Cut);
			draw_menu_action(UI::editor_shortcut_action::DuplicateEntity);
			draw_menu_action(UI::editor_shortcut_action::DeleteEntity);
			ImGui::Separator();
			ImGui::BeginDisabled(!project_loaded);
			if (ImGui::MenuItem("Show Animation Editor"))
				m_animation_editor_panel.open();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Show Test Popup"))
				m_popup_handler.set_show_state(true);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			draw_menu_action(UI::editor_shortcut_action::ReloadScripts, "Reload Assembly");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Project"))
		{
			draw_menu_action(UI::editor_shortcut_action::OpenSettings, "Settings");
			ImGui::Separator();
			draw_menu_action(UI::editor_shortcut_action::OpenProject);
			draw_menu_action(UI::editor_shortcut_action::SaveProject);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			auto draw_panel_toggle = [](const char* label, bool open, auto&& setter)
				{
					bool requested_open = open;
					if (ImGui::MenuItem(label, nullptr, &requested_open))
						setter(requested_open);
				};

			ImGui::BeginDisabled(!project_loaded);
			draw_panel_toggle("Scene Hierarchy", m_scene_hierarchy_panel.is_open(), [this](bool open) { m_scene_hierarchy_panel.set_open(open); });
			draw_panel_toggle("Statistics", m_UI_statistics.is_open(), [this](bool open) { m_UI_statistics.set_open(open); });
			draw_panel_toggle("Animation Editor", m_animation_editor_panel.is_open(), [this](bool open) { m_animation_editor_panel.set_open(open); });
			if (m_content_browser_panel)
				draw_panel_toggle("Content Browser", m_content_browser_panel->is_open(), [this](bool open) { m_content_browser_panel->set_open(open); });
			else
				ImGui::MenuItem("Content Browser", nullptr, false, false);
			ImGui::EndDisabled();
			draw_panel_toggle("Console", console_panel::is_open(), [](bool open) { console_panel::set_open(open); });
			ImGui::EndMenu();
		}

        ImGui::EndMenuBar();
    }
	if (!project_loaded)
	{
		m_project_loader.run();
		ImGui::End(); // dockspace
		console_panel::on_imgui_render();
		m_popup_handler.on_imgui_render();
		if (console_panel::consume_open_dirty())
			save_editor_preferences();
		return;
	}
	// viewport
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Viewport");
		auto viewport_min_region = ImGui::GetWindowContentRegionMin();
		auto viewport_max_region = ImGui::GetWindowContentRegionMax();
		auto viewport_offset = ImGui::GetWindowPos();
		m_viewport_bounds[0] = { viewport_min_region.x + viewport_offset.x, viewport_min_region.y + viewport_offset.y };
		m_viewport_bounds[1] = { viewport_max_region.x + viewport_offset.x, viewport_max_region.y + viewport_offset.y };
		m_viewport_focused = ImGui::IsWindowFocused();
		m_viewport_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		application::get().get_imgui_layer()->block_events(!m_viewport_hovered || m_gizmo_hovered || m_gizmo_using);
		ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
		m_viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

		UI::image(UI::to_imgui_texture_id(m_framebuffer->get_color_attachment_renderer_id()), viewport_panel_size, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });
		UI::drag_drop_target(asset_type::scene, [this](asset_handle handle) { open_scene(handle); }, "scene drag drop", false);

		// gizmos
		entity selected_entity = m_scene_hierarchy_panel.get_selected_entity();
		if (selected_entity && m_gizmo_type != -1 && m_scene_state != scene_state::play)
		{
		    ImGuizmo::SetDrawlist();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::AllowAxisFlip(false);
		    ImGuizmo::SetRect(m_viewport_bounds[0].x, m_viewport_bounds[0].y, m_viewport_bounds[1].x - m_viewport_bounds[0].x, m_viewport_bounds[1].y - m_viewport_bounds[0].y);
		    // Camera
		    const glm::mat4& camera_projection = m_editor_camera.get_projection();
		    glm::mat4 camera_view = m_editor_camera.get_view_matrix();

		    // Entity transform
		    auto& tc = selected_entity.get_component<transform_component>();
		    glm::mat4 transform = tc.get_transform();
			const glm::vec3 base_translation = tc.translation;
			const glm::vec3 base_rotation = tc.rotation;
			const glm::vec3 base_scale = tc.scale;

		    // Snapping
			const int snap_index = gizmo_snap_index(m_gizmo_type);
		    bool snap = is_control_down() && snap_index != -1;

			ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_gizmo_type);
			ImGuizmo::Manipulate(
				glm::value_ptr(camera_view),
				glm::value_ptr(camera_projection),
				operation,
				ImGuizmo::LOCAL,
				glm::value_ptr(transform),
				nullptr,
				snap ? const_cast<float*>(glm::value_ptr(m_UI_settings.get_snap_values(static_cast<uint32_t>(snap_index)))) : nullptr);
			m_gizmo_hovered = ImGuizmo::IsOver(operation);
			m_gizmo_using = ImGuizmo::IsUsing();

		    if (m_gizmo_using)
		    {
				if (!m_gizmo_history_active)
				{
					capture_scene_history();
					m_gizmo_history_active = true;
				}

		        glm::vec3 translation, rotation, scale;
				if (!math::decompose_transform(transform, translation, rotation, scale))
					WHP_CLIENT_WARN("Transform Decomposing error!");

		        glm::vec3 delta_translation = translation - base_translation;
		        glm::vec3 delta_rotation = rotation - base_rotation;
				glm::vec3 scale_ratio = glm::vec3(1.0f);
				scale_ratio.x = base_scale.x != 0.0f ? scale.x / base_scale.x : 1.0f;
				scale_ratio.y = base_scale.y != 0.0f ? scale.y / base_scale.y : 1.0f;
				scale_ratio.z = base_scale.z != 0.0f ? scale.z / base_scale.z : 1.0f;

				std::vector<entity> selected_entities = m_scene_hierarchy_panel.get_selected_entities();
				if (std::find(selected_entities.begin(), selected_entities.end(), selected_entity) == selected_entities.end())
					selected_entities.push_back(selected_entity);

				for (entity selected : selected_entities)
				{
					if (!selected || !selected.has_component<transform_component>())
						continue;
					if (selected == selected_entity)
						continue;

					auto& selected_transform = selected.get_component<transform_component>();
					selected_transform.translation += delta_translation;
					selected_transform.rotation += delta_rotation;
					selected_transform.scale *= scale_ratio;
				}

		        tc.translation = translation;
		        tc.rotation = rotation;
		        tc.scale = scale;
		    }
		}
		if (!m_gizmo_using)
			m_gizmo_history_active = false;
		UI_toolbar();
		application::get().get_imgui_layer()->block_events(!m_viewport_hovered || m_gizmo_hovered || m_gizmo_using);
		ImGui::End();
		ImGui::PopStyleVar();
	} // viewport

	m_UI_project.on_imgui_render(); // should be in dockspace

    ImGui::End(); // dockspace

	// other renders
	m_UI_statistics.on_imgui_render(m_ts);
    m_scene_hierarchy_panel.on_imgui_render();
    m_animation_editor_panel.on_imgui_render();
	console_panel::on_imgui_render();
	if (m_content_browser_panel)
		m_content_browser_panel->on_imgui_render();
	draw_command_palette();
	if (m_UI_settings.consume_dirty()
		|| m_scene_hierarchy_panel.consume_open_dirty()
		|| m_animation_editor_panel.consume_open_dirty()
		|| m_UI_statistics.consume_open_dirty()
		|| console_panel::consume_open_dirty()
		|| (m_content_browser_panel && m_content_browser_panel->consume_preferences_dirty()))
		save_editor_preferences();
	m_popup_handler.on_imgui_render();

}
_WHP_PRAGMA_WARNING(pop)

void editor_layer::on_event(event& evnt)
{
	if (m_scene_state == scene_state::edit && !m_gizmo_hovered && !m_gizmo_using && application::get().get_imgui_layer()->get_active_widgetID() == 0)
		m_editor_camera.on_event(evnt);
    event_dispatcher dispatcher(evnt);
    dispatcher.dispatch<key_pressed_event>([this](auto&&... args) -> decltype(auto) { return this->on_key_pressed(std::forward<decltype(args)>(args)...); });
    dispatcher.dispatch<mouse_button_pressed_event>([this](auto&&... args) -> decltype(auto) { return this->on_mouse_button_pressed(std::forward<decltype(args)>(args)...); });
}

bool editor_layer::on_key_pressed(key_pressed_event& evnt)
{
    // Shortcuts
    if (evnt.get_repeat_count() > 0)
        return false;

	if (application::get().get_imgui_layer()->get_active_widgetID() != 0)
		return false;

    bool control = input::is_key_down(key::left_control) || input::is_key_down(key::right_control);
    bool shift = input::is_key_down(key::left_shift) || input::is_key_down(key::right_shift);
    bool alt = input::is_key_down(key::left_alt) || input::is_key_down(key::right_alt);

	for (size_t i = 0; i < UI::UI_settings::action_count; ++i)
	{
		UI::editor_shortcut_action action = static_cast<UI::editor_shortcut_action>(i);
		if (m_UI_settings.shortcut_matches(action, evnt.get_keycode(), control, shift, alt))
			return execute_editor_action(action);
	}

    return false;
}

bool editor_layer::execute_editor_action(UI::editor_shortcut_action action)
{
	if (!is_editor_action_available(action))
		return false;

	switch (action)
	{
	case UI::editor_shortcut_action::OpenCommandPalette:
		open_command_palette();
		return true;
	case UI::editor_shortcut_action::OpenSettings:
		m_UI_project.show(UI::UI_project::UI_settings, [this]() -> decltype(auto) { return this->finish_project_settings(); });
		return true;
	case UI::editor_shortcut_action::OpenProject:
		open_project();
		return true;
	case UI::editor_shortcut_action::NewScene:
		new_scene();
		return true;
	case UI::editor_shortcut_action::SaveScene:
		save_scene();
		return true;
	case UI::editor_shortcut_action::SaveSceneAs:
		save_scene_as();
		return true;
	case UI::editor_shortcut_action::SaveProject:
		save_project();
		return true;
	case UI::editor_shortcut_action::CloseScene:
		close_scene();
		return true;
	case UI::editor_shortcut_action::ReloadScripts:
		reload_assembly(true);
		return true;
	case UI::editor_shortcut_action::DuplicateEntity:
		on_duplicated_entity();
		return true;
	case UI::editor_shortcut_action::DeleteEntity:
		on_deleted_entity();
		return true;
	case UI::editor_shortcut_action::Undo:
		undo_scene();
		return true;
	case UI::editor_shortcut_action::Redo:
		redo_scene();
		return true;
	case UI::editor_shortcut_action::SelectAll:
		on_select_all_entities();
		return true;
	case UI::editor_shortcut_action::Copy:
		on_copy_entities();
		return true;
	case UI::editor_shortcut_action::Paste:
		on_paste_entities();
		return true;
	case UI::editor_shortcut_action::Cut:
		on_cut_entities();
		return true;
	case UI::editor_shortcut_action::Play:
		if (m_scene_state == scene_state::edit)
			on_scene_play();
		else if (m_scene_state == scene_state::play)
			on_scene_stop();
		return true;
	case UI::editor_shortcut_action::Simulate:
		if (m_scene_state == scene_state::edit)
			on_scene_simulate();
		else if (m_scene_state == scene_state::simulate)
			on_scene_stop();
		return true;
	case UI::editor_shortcut_action::Stop:
		on_scene_stop();
		return true;
	case UI::editor_shortcut_action::Pause:
		m_active_scene->set_paused(!m_active_scene->is_paused());
		return true;
	case UI::editor_shortcut_action::GizmoNone:
		m_gizmo_type = -1;
		return true;
	case UI::editor_shortcut_action::GizmoTranslate:
		m_gizmo_type = ImGuizmo::OPERATION::TRANSLATE;
		return true;
	case UI::editor_shortcut_action::GizmoRotate:
		m_gizmo_type = ImGuizmo::OPERATION::ROTATE;
		return true;
	case UI::editor_shortcut_action::GizmoScale:
		m_gizmo_type = ImGuizmo::OPERATION::SCALE;
		return true;
	default:
		return false;
	}
}

bool editor_layer::is_editor_action_available(UI::editor_shortcut_action action) const
{
	const bool project_loaded = has_project_loaded();
	const bool edit_mode = m_scene_state == scene_state::edit;
	const bool has_selection = (bool)m_scene_hierarchy_panel.get_selected_entity();

	switch (action)
	{
	case UI::editor_shortcut_action::OpenProject:
	case UI::editor_shortcut_action::OpenCommandPalette:
		return true;
	case UI::editor_shortcut_action::OpenSettings:
		return project_loaded;
	case UI::editor_shortcut_action::NewScene:
	case UI::editor_shortcut_action::SaveScene:
	case UI::editor_shortcut_action::SaveSceneAs:
	case UI::editor_shortcut_action::SaveProject:
	case UI::editor_shortcut_action::CloseScene:
	case UI::editor_shortcut_action::ReloadScripts:
	case UI::editor_shortcut_action::SelectAll:
		return project_loaded && edit_mode;
	case UI::editor_shortcut_action::DuplicateEntity:
	case UI::editor_shortcut_action::DeleteEntity:
	case UI::editor_shortcut_action::Copy:
	case UI::editor_shortcut_action::Cut:
		return project_loaded && edit_mode && has_selection;
	case UI::editor_shortcut_action::Paste:
		return project_loaded && edit_mode && !m_entity_clipboard.empty();
	case UI::editor_shortcut_action::Undo:
		return project_loaded && edit_mode && !m_undo_stack.empty();
	case UI::editor_shortcut_action::Redo:
		return project_loaded && edit_mode && !m_redo_stack.empty();
	case UI::editor_shortcut_action::Play:
		return project_loaded && m_scene_state != scene_state::simulate;
	case UI::editor_shortcut_action::Simulate:
		return project_loaded && m_scene_state != scene_state::play;
	case UI::editor_shortcut_action::Stop:
		return m_scene_state == scene_state::play || m_scene_state == scene_state::simulate;
	case UI::editor_shortcut_action::Pause:
		return project_loaded && m_scene_state != scene_state::edit;
	case UI::editor_shortcut_action::GizmoNone:
	case UI::editor_shortcut_action::GizmoTranslate:
	case UI::editor_shortcut_action::GizmoRotate:
	case UI::editor_shortcut_action::GizmoScale:
		return project_loaded && edit_mode && !m_gizmo_using;
	default:
		return false;
	}
}

void editor_layer::open_command_palette()
{
	m_command_palette_open = true;
	m_command_palette_focus_search = true;
	m_command_palette_filter[0] = '\0';
}

void editor_layer::draw_command_palette()
{
	if (!m_command_palette_open)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.22f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(680.0f, 460.0f), ImGuiCond_Appearing);

	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Command Palette", &open, flags))
	{
		if (m_command_palette_focus_search)
		{
			ImGui::SetKeyboardFocusHere();
			m_command_palette_focus_search = false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##CommandPaletteSearch", "Search commands...", m_command_palette_filter, sizeof(m_command_palette_filter));
		ImGui::Spacing();
		ImGui::Separator();

		UI::editor_shortcut_action first_available_action = UI::editor_shortcut_action::Count;
		bool has_visible_command = false;

		if (ImGui::BeginChild("##CommandPaletteResults", ImVec2(0.0f, 0.0f), false))
		{
			if (ImGui::BeginTable("##CommandPaletteTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);

				for (UI::editor_shortcut_action action : command_palette_actions)
				{
					if (!command_matches_filter(action, m_command_palette_filter))
						continue;

					has_visible_command = true;
					const bool available = is_editor_action_available(action);
					if (available && first_available_action == UI::editor_shortcut_action::Count)
						first_available_action = action;

					ImGui::PushID(static_cast<int>(action));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(!available);
					if (ImGui::Selectable(UI::UI_settings::get_action_display_name(action), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 30.0f)))
					{
						if (execute_editor_action(action) && action != UI::editor_shortcut_action::OpenCommandPalette)
							m_command_palette_open = false;
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", UI::UI_settings::get_action_category(action));
					ImGui::TableNextColumn();
					const std::string shortcut = m_UI_settings.get_shortcut_label(action);
					if (m_UI_settings.has_shortcut_conflict(action))
						ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Conflict");
					else
						ImGui::TextDisabled("%s", shortcut.c_str());
					ImGui::EndDisabled();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if (!has_visible_command)
				ImGui::TextDisabled("No commands found.");
		}
		ImGui::EndChild();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			m_command_palette_open = false;
		if (first_available_action != UI::editor_shortcut_action::Count && ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (execute_editor_action(first_available_action) && first_available_action != UI::editor_shortcut_action::OpenCommandPalette)
				m_command_palette_open = false;
		}
	}
	ImGui::End();

	if (!open)
		m_command_palette_open = false;
}

bool editor_layer::on_mouse_button_pressed(mouse_button_pressed_event& evnt)
{
    if (evnt.get_mouse_button() == mouse::button_left)
    {
        if (m_viewport_hovered && !m_gizmo_hovered && !m_gizmo_using && !input::is_key_down(key::left_alt) && application::get().get_imgui_layer()->get_active_widgetID() == 0)
		{
			bool append = is_control_down();
            m_scene_hierarchy_panel.set_selected_entity(m_hovered_entity, append);
		}
    }
    return false;
}

bool editor_layer::on_window_drop(window_drop_event& evnt)
{
	return true;
}

void editor_layer::draw_editor_grid()
{
	if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f)
		return;

	const float aspect_ratio = m_viewport_size.x / m_viewport_size.y;
	const float distance = glm::max(m_editor_camera.get_distance(), 1.0f);
	const float visible_height = distance * 1.15f;
	const float visible_width = visible_height * aspect_ratio;
	const glm::vec3 center = m_editor_camera.get_position() + m_editor_camera.get_forward_direction() * distance;

	float grid_step = 1.0f;
	const float visible_span = glm::max(visible_width, visible_height);
	while ((visible_span / grid_step) > 240.0f)
		grid_step *= 2.0f;

	const int min_x = static_cast<int>(std::floor((center.x - visible_width * 0.5f) / grid_step)) - 2;
	const int max_x = static_cast<int>(std::ceil((center.x + visible_width * 0.5f) / grid_step)) + 2;
	const int min_y = static_cast<int>(std::floor((center.y - visible_height * 0.5f) / grid_step)) - 2;
	const int max_y = static_cast<int>(std::ceil((center.y + visible_height * 0.5f) / grid_step)) + 2;

	const glm::vec4 grid_color{ 0.26f, 0.29f, 0.30f, 0.34f };
	const glm::vec4 major_grid_color{ 0.37f, 0.41f, 0.41f, 0.45f };
	const glm::vec4 x_axis_color{ 0.86f, 0.34f, 0.30f, 0.74f };
	const glm::vec4 y_axis_color{ 0.30f, 0.66f, 0.46f, 0.74f };
	const float min_z = -0.02f;
	auto is_major_grid_line = [](float value)
	{
		return std::fmod(std::abs(value), 10.0f) < 0.0001f;
	};

	renderer2D::begin_scene(m_editor_camera);
	renderer2D::set_line_width(1.0f);

	for (int x = min_x; x <= max_x; ++x)
	{
		const float world_x = static_cast<float>(x) * grid_step;
		const bool is_axis = std::abs(world_x) < 0.0001f;
		const bool is_major = is_major_grid_line(world_x);
		const glm::vec4& color = is_axis ? y_axis_color : (is_major ? major_grid_color : grid_color);
		renderer2D::draw_line(
			{ world_x, static_cast<float>(min_y) * grid_step, min_z },
			{ world_x, static_cast<float>(max_y) * grid_step, min_z },
			color);
	}

	for (int y = min_y; y <= max_y; ++y)
	{
		const float world_y = static_cast<float>(y) * grid_step;
		const bool is_axis = std::abs(world_y) < 0.0001f;
		const bool is_major = is_major_grid_line(world_y);
		const glm::vec4& color = is_axis ? x_axis_color : (is_major ? major_grid_color : grid_color);
		renderer2D::draw_line(
			{ static_cast<float>(min_x) * grid_step, world_y, min_z },
			{ static_cast<float>(max_x) * grid_step, world_y, min_z },
			color);
	}

	renderer2D::end_scene();
}

void editor_layer::on_overlay_render()
{
	WHP_PROFILE_FUNCTION();
	if (m_scene_state == scene_state::play)
	{
		entity cam = m_active_scene->get_primary_camera_entity();
		if (!cam)
			return;
		renderer2D::begin_scene(cam.get_component<camera_component>().camera, cam.get_component<transform_component>().get_transform());
	}
	else
	{
		renderer2D::begin_scene(m_editor_camera);
	}

	if (m_UI_settings.get_show_physics_colliders())
	{
		// Box Colliders
		{
			auto view = m_active_scene->get_all_entities_with<transform_component, box_collider2D_component>();
			for (auto entity : view)
			{
				auto [tc, bc2d] = view.get<transform_component, box_collider2D_component>(entity);

				glm::vec3 translation = tc.translation + glm::vec3(bc2d.offset, 0.001f);
				glm::vec3 scale = tc.scale * glm::vec3(bc2d.size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.translation)
					* glm::rotate(glm::mat4(1.0f), tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.offset, 0.001f))
					* glm::scale(glm::mat4(1.0f), scale);

				renderer2D::draw_rect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders
		{
			auto view = m_active_scene->get_all_entities_with<transform_component, circle_collider2D_component>();
			for (auto entity : view)
			{
				auto [tc, cc2d] = view.get<transform_component, circle_collider2D_component>(entity);

				glm::vec3 translation = tc.translation + glm::vec3(cc2d.offset, 0.001f);
				glm::vec3 scale = tc.scale * glm::vec3(cc2d.radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				renderer2D::draw_circle(transform, glm::vec4(0, 1, 0, 1), 0.02f);
			}
		}
	}

	for (entity selected_entity : m_scene_hierarchy_panel.get_selected_entities())
	{
		transform_component transform = selected_entity.get_component<transform_component>();
		if (selected_entity.has_component<text_component>() && !selected_entity.has_component<sprite_renderer_component>() && !selected_entity.has_component<circle_renderer_component>())
		{
			selected_entity.get_component<text_component>();
		}
		else
			renderer2D::draw_rect(transform.get_transform(), glm::vec4(0.9f, 0.4f, 0.1f, 1.0f));
	}

	renderer2D::end_scene();
}

bool editor_layer::has_project_loaded() const
{
	return project::get_active() != nullptr;
}

void editor_layer::setup_project_loader()
{
	m_project_loader.set_create_project_callback([this](const UI::project_create_settings& settings) { return new_project(settings); });
	m_project_loader.set_load_project_callback([this]() { return open_project(); });
	m_project_loader.set_open_recent_project_callback([this](const std::filesystem::path& path) {
		std::error_code error;
		if (!std::filesystem::exists(path, error))
		{
			WHP_EDITOR_WARN("[Whip Hub] Recent project no longer exists.");
			load_recent_projects();
			return false;
		}

		return open_project(path);
	});
	m_project_loader.set_forget_recent_project_callback([this](const std::filesystem::path& path) {
		return forget_recent_project(path);
	});
	m_project_loader.set_delete_recent_project_callback([this](const std::filesystem::path& path) {
		return delete_recent_project(path);
	});
}

void editor_layer::load_recent_projects()
{
	m_recent_projects.clear();
	bool should_rewrite = false;

	std::ifstream stream(get_recent_projects_path());
	if (!stream)
	{
		m_project_loader.set_recent_projects(m_recent_projects);
		return;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		if (line.empty())
			continue;

		std::filesystem::path path(line);
		std::error_code error;
		if (!std::filesystem::exists(path, error) || path.extension() != ".wproj")
		{
			should_rewrite = true;
			continue;
		}

		path = std::filesystem::weakly_canonical(path, error);
		if (error)
			path = std::filesystem::absolute(line, error);
		if (!should_include_recent_project(path))
		{
			should_rewrite = true;
			continue;
		}

		if (std::find(m_recent_projects.begin(), m_recent_projects.end(), path) == m_recent_projects.end())
			m_recent_projects.push_back(path);
		else
			should_rewrite = true;

		if (m_recent_projects.size() >= 10)
		{
			should_rewrite = true;
			break;
		}
	}

	stream.close();
	if (should_rewrite)
		save_recent_projects();
	m_project_loader.set_recent_projects(m_recent_projects);
}

void editor_layer::save_recent_projects() const
{
	std::ofstream stream(get_recent_projects_path(), std::ios::trunc);
	if (!stream)
		return;

	for (const auto& project_path : m_recent_projects)
		stream << project_path.string() << '\n';
}

void editor_layer::add_recent_project(const std::filesystem::path& path)
{
	if (path.empty())
		return;

	std::error_code error;
	std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, error);
	if (error)
	{
		error.clear();
		normalized_path = std::filesystem::absolute(path, error);
	}
	if (error)
		normalized_path = path;
	if (!should_include_recent_project(normalized_path))
		return;

	m_recent_projects.erase(
		std::remove(m_recent_projects.begin(), m_recent_projects.end(), normalized_path),
		m_recent_projects.end());

	m_recent_projects.insert(m_recent_projects.begin(), normalized_path);
	if (m_recent_projects.size() > 10)
		m_recent_projects.resize(10);

	m_last_project_path = normalized_path;
	save_recent_projects();
	save_editor_preferences();
	m_project_loader.set_recent_projects(m_recent_projects);
}

bool editor_layer::forget_recent_project(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	const size_t previous_size = m_recent_projects.size();
	m_recent_projects.erase(
		std::remove_if(m_recent_projects.begin(), m_recent_projects.end(),
			[&path](const std::filesystem::path& recent_path)
			{
				return paths_match_for_recent_project(recent_path, path);
			}),
		m_recent_projects.end());

	if (m_recent_projects.size() == previous_size)
		return false;

	if (paths_match_for_recent_project(m_last_project_path, path))
		m_last_project_path.clear();

	save_recent_projects();
	save_editor_preferences();
	m_project_loader.set_recent_projects(m_recent_projects);
	return true;
}

bool editor_layer::delete_recent_project(const std::filesystem::path& path)
{
	if (path.empty() || path.extension() != ".wproj")
		return false;

	std::error_code error;
	std::filesystem::path project_path = normalize_project_list_path(path);
	const bool project_file_exists = std::filesystem::exists(project_path, error);
	if (error)
		return false;
	if (!project_file_exists)
		return forget_recent_project(path);

	if (!std::filesystem::is_regular_file(project_path, error) || error)
		return false;

	const std::filesystem::path project_directory = normalize_project_list_path(project_path.parent_path());
	if (project_directory.empty() || project_directory == project_directory.root_path())
		return false;

	if (project_directory.filename() != project_path.stem())
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete project folder because it does not match the project file name: ") + project_directory.string());
		return false;
	}

	error.clear();
	const std::filesystem::path working_directory = normalize_project_list_path(std::filesystem::current_path());
	if (!working_directory.empty() && path_is_or_is_under(working_directory, project_directory))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Refusing to delete a project folder that contains the editor working directory: ") + project_directory.string());
		return false;
	}

	ref<project> active_project = project::get_active();
	if (active_project && paths_match_for_recent_project(active_project->get_project_path(), project_path))
	{
		WHP_EDITOR_WARN("[Whip Hub] Refusing to delete the currently loaded project.");
		return false;
	}

	std::filesystem::remove_all(project_directory, error);
	if (error)
	{
		WHP_EDITOR_ERROR(std::string("[Whip Hub] Could not delete project folder ") + project_directory.string() + ": " + error.message());
		return false;
	}

	return forget_recent_project(project_path);
}

bool editor_layer::should_include_recent_project(const std::filesystem::path& path) const
{
	std::error_code error;
	std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, error);
	if (error)
		normalized_path = path;

	error.clear();
	std::filesystem::path working_directory = std::filesystem::weakly_canonical(std::filesystem::current_path(), error);
	if (error)
		working_directory = std::filesystem::current_path();

	error.clear();
	std::filesystem::path relative_path = std::filesystem::relative(normalized_path, working_directory, error);
	if (!error && !relative_path.empty())
	{
		const std::filesystem::path first_component = *relative_path.begin();
		if (first_component != ".." && first_component != ".")
			return false;
	}

	return true;
}

std::filesystem::path editor_layer::get_recent_projects_path() const
{
	return std::filesystem::current_path() / "WhipHubRecentProjects.txt";
}

std::filesystem::path editor_layer::get_preferences_path() const
{
	return std::filesystem::current_path() / "WhipEditorPreferences.yaml";
}

void editor_layer::load_editor_preferences()
{
	load_recent_projects();

	const std::filesystem::path preferences_path = get_preferences_path();
	std::error_code error;
	if (!std::filesystem::exists(preferences_path, error))
		return;

	YAML::Node data;
	try
	{
		data = YAML::LoadFile(preferences_path.string());
	}
	catch (const YAML::Exception& exception)
	{
		WHP_EDITOR_WARN(std::string("[Editor Preferences] Could not read preferences: ") + exception.what());
		return;
	}

	if (YAML::Node recent_projects = data["recent_projects"])
	{
		m_recent_projects.clear();
		for (const YAML::Node& recent_project : recent_projects)
		{
			std::filesystem::path path = recent_project.as<std::string>("");
			if (!path.empty() && should_include_recent_project(path))
				m_recent_projects.push_back(path);
		}
	}

	m_last_project_path = data["last_project"].as<std::string>("");

	if (YAML::Node editor = data["editor"])
	{
		m_UI_settings.set_show_physics_colliders(editor["show_physics_colliders"].as<bool>(m_UI_settings.get_show_physics_colliders()));
		m_UI_settings.set_step_frame(editor["step_frame"].as<int>(m_UI_settings.get_step_frame()));
		m_UI_settings.set_theme(theme_from_string(editor["theme"].as<std::string>(UI::UI_settings::get_theme_name(m_UI_settings.get_theme()))));

		if (YAML::Node snap = editor["snap"])
		{
			m_UI_settings.set_snap_values(0, read_vec3(snap["translation"], m_UI_settings.get_snap_values(0)));
			m_UI_settings.set_snap_values(1, read_vec3(snap["rotation"], m_UI_settings.get_snap_values(1)));
			m_UI_settings.set_snap_values(2, read_vec3(snap["scale"], m_UI_settings.get_snap_values(2)));
		}

		if (YAML::Node shortcuts = editor["shortcuts"])
		{
			for (size_t i = 0; i < UI::UI_settings::action_count; ++i)
			{
				UI::editor_shortcut_action action = static_cast<UI::editor_shortcut_action>(i);
				YAML::Node shortcut = shortcuts[UI::UI_settings::get_action_storage_key(action)];
				if (!shortcut)
					continue;

				UI::shortcut_binding binding;
				binding.key = static_cast<key_code>(shortcut["key"].as<int>(0));
				binding.ctrl = shortcut["ctrl"].as<bool>(false);
				binding.shift = shortcut["shift"].as<bool>(false);
				binding.alt = shortcut["alt"].as<bool>(false);
				m_UI_settings.set_shortcut_binding(action, binding);
			}
		}
	}

	if (YAML::Node panels = data["panels"])
	{
		m_animation_editor_panel.set_open(panels["animation_editor"].as<bool>(m_animation_editor_panel.is_open()));
		m_scene_hierarchy_panel.set_open(panels["scene_hierarchy"].as<bool>(m_scene_hierarchy_panel.is_open()));
		m_UI_statistics.set_open(panels["statistics"].as<bool>(m_UI_statistics.is_open()));
		console_panel::set_open(panels["console"].as<bool>(console_panel::is_open()));
	}

	if (YAML::Node browser = data["content_browser"])
	{
		m_content_browser_preferences.thumbnail_size = browser["thumbnail_size"].as<float>(m_content_browser_preferences.thumbnail_size);
		m_content_browser_preferences.padding = browser["padding"].as<float>(m_content_browser_preferences.padding);
		m_content_browser_preferences.show_unsupported = browser["show_unsupported"].as<bool>(m_content_browser_preferences.show_unsupported);
		m_content_browser_preferences.open = browser["open"].as<bool>(m_content_browser_preferences.open);
		m_content_browser_preferences.mode = browser["mode"].as<int>(m_content_browser_preferences.mode);
		m_content_browser_preferences.type_filter = browser["type_filter"].as<int>(m_content_browser_preferences.type_filter);
		m_content_browser_preferences.current_directory = browser["current_directory"].as<std::string>("");
		m_has_content_browser_preferences = true;
	}

	m_UI_settings.consume_dirty();
	m_scene_hierarchy_panel.consume_open_dirty();
	m_animation_editor_panel.consume_open_dirty();
	m_UI_statistics.consume_open_dirty();
	console_panel::consume_open_dirty();
	m_project_loader.set_recent_projects(m_recent_projects);
}

void editor_layer::save_editor_preferences() const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "last_project" << YAML::Value << m_last_project_path.string();
	out << YAML::Key << "recent_projects" << YAML::Value << YAML::BeginSeq;
	for (const auto& project_path : m_recent_projects)
		out << project_path.string();
	out << YAML::EndSeq;

	out << YAML::Key << "editor" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "show_physics_colliders" << YAML::Value << m_UI_settings.get_show_physics_colliders();
	out << YAML::Key << "step_frame" << YAML::Value << m_UI_settings.get_step_frame();
	out << YAML::Key << "theme" << YAML::Value << UI::UI_settings::get_theme_name(m_UI_settings.get_theme());
	out << YAML::Key << "snap" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "translation" << YAML::Value; write_vec3(out, m_UI_settings.get_snap_values(0));
	out << YAML::Key << "rotation" << YAML::Value; write_vec3(out, m_UI_settings.get_snap_values(1));
	out << YAML::Key << "scale" << YAML::Value; write_vec3(out, m_UI_settings.get_snap_values(2));
	out << YAML::EndMap;
	out << YAML::Key << "shortcuts" << YAML::Value << YAML::BeginMap;
	for (size_t i = 0; i < UI::UI_settings::action_count; ++i)
	{
		UI::editor_shortcut_action action = static_cast<UI::editor_shortcut_action>(i);
		UI::shortcut_binding binding = m_UI_settings.get_shortcut_binding(action);
		out << YAML::Key << UI::UI_settings::get_action_storage_key(action) << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << binding.key;
		out << YAML::Key << "ctrl" << YAML::Value << binding.ctrl;
		out << YAML::Key << "shift" << YAML::Value << binding.shift;
		out << YAML::Key << "alt" << YAML::Value << binding.alt;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;

	out << YAML::Key << "panels" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "animation_editor" << YAML::Value << m_animation_editor_panel.is_open();
	out << YAML::Key << "scene_hierarchy" << YAML::Value << m_scene_hierarchy_panel.is_open();
	out << YAML::Key << "statistics" << YAML::Value << m_UI_statistics.is_open();
	out << YAML::Key << "console" << YAML::Value << console_panel::is_open();
	out << YAML::EndMap;

	content_browser_panel::preferences browser_preferences = m_content_browser_panel ? m_content_browser_panel->get_preferences() : m_content_browser_preferences;
	out << YAML::Key << "content_browser" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "thumbnail_size" << YAML::Value << browser_preferences.thumbnail_size;
	out << YAML::Key << "padding" << YAML::Value << browser_preferences.padding;
	out << YAML::Key << "show_unsupported" << YAML::Value << browser_preferences.show_unsupported;
	out << YAML::Key << "open" << YAML::Value << browser_preferences.open;
	out << YAML::Key << "mode" << YAML::Value << browser_preferences.mode;
	out << YAML::Key << "type_filter" << YAML::Value << browser_preferences.type_filter;
	out << YAML::Key << "current_directory" << YAML::Value << browser_preferences.current_directory.string();
	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream stream(get_preferences_path(), std::ios::trunc);
	if (!stream)
		return;
	stream << out.c_str();
}

void editor_layer::apply_preferences_to_content_browser()
{
	if (m_content_browser_panel && m_has_content_browser_preferences)
		m_content_browser_panel->apply_preferences(m_content_browser_preferences);
}

bool editor_layer::new_project(const UI::project_create_settings& settings)
{
	const std::string project_name = sanitize_project_token(settings.name, "Untitled");
	const std::string project_folder_name = sanitize_path_token(project_name, "Untitled");
	const std::string initial_scene_name = sanitize_path_token(settings.initial_scene_name, "Main");
	if (settings.location.empty())
		return false;

	std::filesystem::path project_directory = settings.location / project_folder_name;
	std::filesystem::path project_path = project_directory / (project_folder_name + ".wproj");
	std::error_code error;
	if (std::filesystem::exists(project_path, error))
	{
		WHP_EDITOR_WARN(std::string("[Whip Hub] Project file already exists: ") + project_path.string());
		return false;
	}

	if (!create_directory_checked(project_directory / "Assets" / "Scenes", "project scenes directory") ||
		!create_directory_checked(project_directory / "Assets" / "Scripts" / "Source", "script source directory") ||
		!create_directory_checked(project_directory / "Assets" / "Scripts" / "Binaries", "script binaries directory") ||
		!create_directory_checked(project_directory / "Assets" / "Animations", "animations directory") ||
		!create_directory_checked(project_directory / "Assets" / "Audios", "audio directory") ||
		!create_directory_checked(project_directory / "Assets" / "fonts", "font directory") ||
		!create_directory_checked(project_directory / "Assets" / "textures", "texture directory"))
	{
		return false;
	}

	ref<project> new_project = project::new_project();
	project::set_active_project_path(project_path);

	project_config& config = new_project->get_config();
	config.name = project_name;
	config.asset_directory = "Assets";
	config.asset_registry_path = "asset_registry.whipr";
	config.script_module_path = std::filesystem::path("Scripts") / "Binaries" / (project_folder_name + ".dll");
	config.start_scene = 0;

	if (!write_script_project_files(project_directory, project_folder_name))
	{
		project::set_active(nullptr);
		return false;
	}

	std::filesystem::path start_scene_relative_path;
	asset_handle start_scene_handle = 0;
	if (settings.create_start_scene)
	{
		start_scene_handle = asset_handle{};
		config.start_scene = start_scene_handle;
		start_scene_relative_path = std::filesystem::path("Scenes") / (initial_scene_name + ".whip");

		ref<scene> start_scene = make_ref<scene>(start_scene_handle);
		if (settings.template_index == 1 || settings.template_index == 2)
		{
			entity camera = start_scene->create_entity("Main Camera");
			camera.add_component<camera_component>();
			camera.get_component<transform_component>().translation = { 0.0f, 0.0f, 8.0f };

			entity sprite = start_scene->create_entity(settings.template_index == 2 ? "Starter Entity" : "Sprite");
			sprite.add_component<sprite_renderer_component>(glm::vec4{ 0.86f, 0.58f, 0.28f, 1.0f });
			if (settings.template_index == 2)
				sprite.add_component<script_component>().class_name = project_folder_name + ".StarterEntity";
		}

		scene_importer::save_scene(start_scene, start_scene_relative_path);
	}

	if (!project::save_active(project_path))
	{
		project::set_active(nullptr);
		return false;
	}

	std::ofstream registry(project_path.parent_path() / config.asset_directory / config.asset_registry_path, std::ios::trunc);
	if (!registry)
	{
		WHP_EDITOR_ERROR("[Whip Hub] Could not write asset registry.");
		project::set_active(nullptr);
		return false;
	}

	if (settings.create_start_scene)
	{
		registry << "asset_registry:\n";
		registry << "  - handle: " << (uint64_t)start_scene_handle << '\n';
		registry << "    filepath: " << start_scene_relative_path.generic_string() << '\n';
		registry << "    type: scene\n";
	}
	else
	{
		registry << "asset_registry: []\n";
	}
	registry.close();

	project::set_active(nullptr);
	if (!settings.open_after_create)
	{
		add_recent_project(project_path);
		return true;
	}
	return open_project(project_path);
}

void editor_layer::save_project()
{
	if (!has_project_loaded())
		return;

	project::save_active();
}

void editor_layer::finish_project_settings()
{
	if (!has_project_loaded())
		return;

	project::save_active();
	reload_assembly(true);
	m_content_browser_panel = make_scope<content_browser_panel>(project::get_active());
	apply_preferences_to_content_browser();
}


bool editor_layer::open_project()
{
	std::string filepath = file_dialogs::open_file("Whip Project (*.wproj)\0*.wproj\0");
	if (filepath.empty())
		return false;
	return open_project(filepath);
}

bool editor_layer::open_project(const std::filesystem::path& path)
{
	if (has_project_loaded())
	{
		if (m_scene_state != scene_state::edit)
			on_scene_stop();
		m_content_browser_panel.reset();
		m_scene_hierarchy_panel.set_context({});
		m_editor_scene = make_ref<scene>();
		m_active_scene = m_editor_scene;
		m_editor_scene_path.clear();
		clear_scene_history();
	}

	if (project::load(path))
	{
		if (!build_project_scripts())
			WHP_EDITOR_WARN("[Script Build] Project opened, but script build failed.");
		script_engine::init();
		asset_handle start_scene = (project::get_active()->get_config().start_scene);
		if (start_scene && project::get_active()->get_editor_asset_manager()->is_asset_handle_valid(start_scene))
		{
			const std::filesystem::path start_scene_path = project::get_active()->get_editor_asset_manager()->get_filepath(start_scene);
			if (std::filesystem::exists(project::get_active_asset_directory() / start_scene_path))
				open_scene(start_scene);
			else
			{
				WHP_EDITOR_WARN("[Project] Start scene file is missing. Resetting project start scene.");
				project::get_active()->get_config().start_scene = 0;
				project::save_active();
			}
		}
		else if (start_scene)
		{
			WHP_EDITOR_WARN("[Project] Start scene is missing. Resetting project start scene.");
			project::get_active()->get_config().start_scene = 0;
			project::save_active();
		}
		else
		{
			new_scene();
		}
		m_content_browser_panel = make_scope<content_browser_panel>(project::get_active());
		apply_preferences_to_content_browser();
		add_recent_project(path);
		m_project_loader.set_loaded(true);
		return true;
	}
	return false;
}

void editor_layer::new_scene()
{
	if (!has_project_loaded())
		return;

    m_active_scene = make_ref<scene>();
	m_scene_hierarchy_panel.set_context(m_active_scene);
	m_editor_scene_path = std::filesystem::path();
	clear_scene_history();
}

void editor_layer::open_scene(asset_handle handle)
{
	if (!has_project_loaded())
		return;

	if (m_scene_state != scene_state::edit)
		on_scene_stop();

	if (!project::get_active()->get_editor_asset_manager()->is_asset_handle_valid(handle))
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset handle is not registered.");
		return;
	}
	const std::filesystem::path scene_path = project::get_active()->get_editor_asset_manager()->get_filepath(handle);
	if (!std::filesystem::exists(project::get_active_asset_directory() / scene_path))
	{
		WHP_EDITOR_WARN(std::string("[Scene] Failed to open scene. File is missing: ") + scene_path.string());
		return;
	}

	ref<scene> read_only_scene = asset_manager::get_asset<scene>(handle);
	if (!read_only_scene)
	{
		WHP_EDITOR_WARN("[Scene] Failed to open scene. Asset is missing or failed to import.");
		return;
	}
	ref<scene> new_scene = scene::copy(read_only_scene);

	m_editor_scene = new_scene;
	m_scene_hierarchy_panel.set_context(m_editor_scene);

	m_active_scene = m_editor_scene;
	m_editor_scene_path = scene_path;
	clear_scene_history();
}

void editor_layer::close_scene()
{
	if (m_scene_state != scene_state::edit)
		on_scene_stop();
	ref<scene> new_scene = make_ref<scene>();
	m_editor_scene = new_scene;
	m_editor_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
	m_active_scene = m_editor_scene;
	m_editor_scene_path.clear();
	m_scene_hierarchy_panel.set_context({});
	clear_scene_history();
}

void editor_layer::save_scene()
{
	if (!has_project_loaded())
		return;

	if (!m_editor_scene_path.empty())
		serialize_scene(m_active_scene, m_editor_scene_path);
	else
		save_scene_as();
}

void editor_layer::save_scene_as()
{
	if (!has_project_loaded())
		return;

    std::string filepath = file_dialogs::save_file("Whip Scene (*.whip)\0*.whip\0");
    if (!filepath.empty())
    {
		serialize_scene(m_active_scene, filepath);
		m_editor_scene_path = filepath;
    }
}

bool editor_layer::build_project_scripts() const
{
	if (!has_project_loaded())
		return false;

	const project_config& config = project::get_active()->get_config();
	if (config.script_module_path.empty())
	{
		WHP_EDITOR_INFO("[Script Build] Project has no script module configured.");
		return true;
	}

	const std::filesystem::path scripts_directory = project::get_active_asset_directory() / "Scripts";
	const std::string project_folder_name = sanitize_path_token(config.name, "Untitled");
	const std::filesystem::path preferred_project_file = scripts_directory / (project_folder_name + ".csproj");
	const std::filesystem::path preferred_solution_file = scripts_directory / (project_folder_name + ".sln");
	const std::filesystem::path build_props_file = scripts_directory / "Directory.Build.props";
	std::error_code error;
	bool needs_workspace_refresh = !std::filesystem::exists(preferred_project_file, error);
	error.clear();
	needs_workspace_refresh = needs_workspace_refresh || !std::filesystem::exists(preferred_solution_file, error);
	error.clear();
	needs_workspace_refresh = needs_workspace_refresh || !std::filesystem::exists(build_props_file, error);
	if (!needs_workspace_refresh)
	{
		const std::string project_file_contents = read_text_file(preferred_project_file);
		const std::string solution_file_contents = read_text_file(preferred_solution_file);
		const std::string build_props_contents = read_text_file(build_props_file);
		needs_workspace_refresh =
			project_file_contents.find("<Project Sdk=\"Microsoft.NET.Sdk\">") == std::string::npos ||
			project_file_contents.find("<ProjectReference Include=\"Whip-ScriptCore\\Whip-ScriptCore.csproj\">") == std::string::npos ||
			project_file_contents.find("<BaseIntermediateOutputPath>") != std::string::npos ||
			build_props_contents.find("ScriptIntermediates") == std::string::npos ||
			solution_file_contents.find("Whip-ScriptCore\\Whip-ScriptCore.csproj") == std::string::npos;
	}
	if (needs_workspace_refresh)
	{
		WHP_EDITOR_INFO("[Script Build] Refreshing generated C# workspace files.");
		if (!refresh_script_workspace_files(scripts_directory, project_folder_name))
			WHP_EDITOR_WARN("[Script Build] Could not refresh generated C# workspace files.");
	}

	sync_script_core_binary(scripts_directory);

	const std::filesystem::path script_project_file = find_script_project_file(scripts_directory, config.name);
	if (script_project_file.empty())
	{
		WHP_EDITOR_WARN(std::string("[Script Build] No C# project file found under ") + scripts_directory.string() + ". Reloading the existing assembly if it exists.");
		return true;
	}

	const script_build_command build_command = make_script_build_command(script_project_file);
	if (build_command.command.empty())
	{
		WHP_EDITOR_WARN("[Script Build] Could not find MSBuild.exe or dotnet. Set WHIP_MSBUILD_PATH or add MSBuild/dotnet to PATH. Reloading the existing assembly if it exists.");
		return true;
	}

	WHP_EDITOR_INFO(std::string("[Script Build] Building ") + script_project_file.string() + " with " + build_command.tool_name);
	const int result = run_command_and_log_output(build_command.command);
	if (result != 0)
	{
		WHP_EDITOR_ERROR(std::string("[Script Build] Build failed with exit code ") + std::to_string(result) + ".");
		return false;
	}

	WHP_EDITOR_INFO("[Script Build] Build succeeded.");
	return true;
}

void editor_layer::reload_assembly(bool reset_app_assembly_filepath) const
{
	if (!has_project_loaded())
	{
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. No project is loaded.");
		return;
	}

	if (m_scene_state == scene_state::edit)
	{
		if (!build_project_scripts())
		{
			WHP_CORE_WARN("[Script Engine] Assembly reload skipped because script build failed.");
			return;
		}
		assembly_manager::reload_assembly(reset_app_assembly_filepath);
	}
	else
		WHP_CORE_WARN("[Script Engine] Failed to reload assembly. Scene is running or simulating!");
}

void editor_layer::serialize_scene(ref<scene> scene_in, const std::filesystem::path& path)
{
	scene_importer::save_scene(scene_in, path);
}

void editor_layer::on_scene_play()
{
	if (!has_project_loaded())
		return;

	if (m_scene_state == scene_state::simulate)
		on_scene_stop();
	project::run_state(true);
	m_scene_state = scene_state::play;
	script_engine::set_filewatcher_state(false);
	m_active_scene = scene::copy(m_editor_scene);
	m_active_scene->on_runtime_start();
	m_last_selected_entity = m_scene_hierarchy_panel.get_selected_entity();
	m_scene_hierarchy_panel.set_context(m_active_scene);
}

void editor_layer::on_scene_simulate()
{
	if (!has_project_loaded())
		return;

	if (m_scene_state == scene_state::play)
		on_scene_stop();

	project::run_state(true);
	m_scene_state = scene_state::simulate;
	script_engine::set_filewatcher_state(false);
	m_active_scene = scene::copy(m_editor_scene);
	m_active_scene->on_simulation_start();

	m_active_scene->on_runtime_start();
	m_last_selected_entity = m_scene_hierarchy_panel.get_selected_entity();
	m_scene_hierarchy_panel.set_context(m_active_scene);
	// maybe do not ??
	if(m_last_selected_entity)
		m_scene_hierarchy_panel.set_selected_entity(m_active_scene->find_entity_by_UUID(m_last_selected_entity.get_UUID()));
}

void editor_layer::on_scene_stop()
{
	WHP_CORE_ASSERT(m_scene_state == scene_state::play || m_scene_state == scene_state::simulate, "invalid scene_state!");
	project::run_state(false);
	if (m_scene_state == scene_state::play)
		m_active_scene->on_runtime_stop();
	else if (m_scene_state == scene_state::simulate)
		m_active_scene->on_simulation_stop();
	m_scene_state = scene_state::edit;
	script_engine::set_filewatcher_state(true);
	m_active_scene = m_editor_scene;
	m_scene_hierarchy_panel.set_context(m_active_scene);
	m_scene_hierarchy_panel.set_selected_entity(m_last_selected_entity);
}

void editor_layer::on_scene_pause()
{

}

editor_layer::project_history_entry editor_layer::capture_project_history() const
{
	project_history_entry entry;
	ref<project> active_project = project::get_active();
	if (!active_project || !active_project->get_editor_asset_manager())
		return entry;

	entry.valid = true;
	entry.config = active_project->get_config();
	entry.project_path = active_project->get_project_path();
	entry.asset_registry_path = active_project->get_asset_registry_path();
	entry.project_file_contents = read_text_file(entry.project_path);
	entry.asset_registry_contents = read_text_file(entry.asset_registry_path);

	const asset_registry& registry = active_project->get_editor_asset_manager()->get_asset_registry();
	registry.foreach(asset_type::scene, [active_project, &entry](const asset_registry::value_type& value)
		{
			const std::string relative_path = value.second.filepath.generic_string();
			entry.scene_file_contents[relative_path] = read_text_file(active_project->get_asset_directory() / value.second.filepath);
		});

	return entry;
}

void editor_layer::restore_project_history(const project_history_entry& entry)
{
	if (!entry.valid)
		return;

	ref<project> active_project = project::get_active();
	if (!active_project || !active_project->get_editor_asset_manager())
		return;
	if (!entry.project_path.empty() && active_project->get_project_path() != entry.project_path)
		return;

	std::unordered_set<std::string> current_scene_paths;
	const std::filesystem::path current_asset_directory = active_project->get_asset_directory();
	active_project->get_editor_asset_manager()->get_asset_registry().foreach(asset_type::scene, [&current_scene_paths](const asset_registry::value_type& value)
		{
			current_scene_paths.insert(value.second.filepath.generic_string());
		});

	active_project->get_config() = entry.config;
	if (!entry.project_file_contents.empty())
		write_text_file(entry.project_path, entry.project_file_contents);
	else
		project::save_active();

	const std::filesystem::path restored_asset_directory = entry.project_path.parent_path() / entry.config.asset_directory;
	const std::filesystem::path restored_asset_registry_path = restored_asset_directory / entry.config.asset_registry_path;
	if (!entry.asset_registry_contents.empty())
		write_text_file(restored_asset_registry_path, entry.asset_registry_contents);

	for (const auto& [relative_path, contents] : entry.scene_file_contents)
		write_text_file(restored_asset_directory / relative_path, contents);

	for (const std::string& relative_path : current_scene_paths)
	{
		if (entry.scene_file_contents.find(relative_path) != entry.scene_file_contents.end())
			continue;

		std::error_code error;
		std::filesystem::remove(current_asset_directory / relative_path, error);
		if (current_asset_directory != restored_asset_directory)
			std::filesystem::remove(restored_asset_directory / relative_path, error);
	}

	active_project->get_editor_asset_manager()->deserialize_asset_registry();
	if (m_content_browser_panel)
	{
		m_content_browser_panel = make_scope<content_browser_panel>(active_project);
		apply_preferences_to_content_browser();
	}
}

void editor_layer::capture_scene_history(bool include_project_snapshot)
{
	if (m_scene_state != scene_state::edit || !m_editor_scene)
		return;

	scene_history_entry entry;
	entry.scene_snapshot = scene::copy(m_editor_scene);
	entry.editor_scene_path = m_editor_scene_path;
	entry.selected_entities = m_scene_hierarchy_panel.get_selected_entity_ids();
	if (include_project_snapshot)
		entry.project_snapshot = capture_project_history();
	m_undo_stack.push_back(entry);
	m_redo_stack.clear();

	static constexpr size_t max_history_entries = 64;
	if (m_undo_stack.size() > max_history_entries)
		m_undo_stack.erase(m_undo_stack.begin());
}

void editor_layer::restore_scene_history(const scene_history_entry& entry)
{
	if (!entry.scene_snapshot)
		return;

	if (m_scene_state != scene_state::edit)
		on_scene_stop();

	restore_project_history(entry.project_snapshot);
	m_editor_scene = scene::copy(entry.scene_snapshot);
	m_editor_scene_path = entry.editor_scene_path;
	m_editor_scene->on_viewport_resize((uint32_t)m_viewport_size.x, (uint32_t)m_viewport_size.y);
	m_active_scene = m_editor_scene;
	m_scene_hierarchy_panel.set_context(m_editor_scene);
	m_scene_hierarchy_panel.set_selected_entity_ids(entry.selected_entities);
}

void editor_layer::undo_scene()
{
	if (m_undo_stack.empty() || m_scene_state != scene_state::edit)
		return;

	scene_history_entry current;
	current.scene_snapshot = scene::copy(m_editor_scene);
	current.editor_scene_path = m_editor_scene_path;
	current.selected_entities = m_scene_hierarchy_panel.get_selected_entity_ids();
	scene_history_entry entry = m_undo_stack.back();
	if (entry.project_snapshot.valid)
		current.project_snapshot = capture_project_history();
	m_redo_stack.push_back(current);

	m_undo_stack.pop_back();
	restore_scene_history(entry);
}

void editor_layer::redo_scene()
{
	if (m_redo_stack.empty() || m_scene_state != scene_state::edit)
		return;

	scene_history_entry current;
	current.scene_snapshot = scene::copy(m_editor_scene);
	current.editor_scene_path = m_editor_scene_path;
	current.selected_entities = m_scene_hierarchy_panel.get_selected_entity_ids();
	scene_history_entry entry = m_redo_stack.back();
	if (entry.project_snapshot.valid)
		current.project_snapshot = capture_project_history();
	m_undo_stack.push_back(current);

	m_redo_stack.pop_back();
	restore_scene_history(entry);
}

void editor_layer::clear_scene_history()
{
	m_undo_stack.clear();
	m_redo_stack.clear();
	m_gizmo_history_active = false;
}

void editor_layer::on_duplicated_entity()
{
	if (m_scene_state != scene_state::edit)
		return;

	std::vector<entity> selected_entities = m_scene_hierarchy_panel.get_selected_entities();
	if (selected_entities.empty())
		return;

	capture_scene_history();
	bool append = false;
	for (entity selected_entity : selected_entities)
	{
		entity duplicated = m_editor_scene->duplicate_entity(selected_entity);
		m_scene_hierarchy_panel.set_selected_entity(duplicated, append);
		append = true;
	}
}

void editor_layer::on_deleted_entity()
{
	if(application::get().get_imgui_layer()->get_active_widgetID() == 0)
	{
		std::vector<entity> selected_entities = m_scene_hierarchy_panel.get_selected_entities();
		if (!selected_entities.empty())
		{
			capture_scene_history();
			std::vector<UUID> selected_ids;
			selected_ids.reserve(selected_entities.size());
			for (entity selected_entity : selected_entities)
				selected_ids.push_back(selected_entity.get_UUID());

			auto has_selected_ancestor = [&](entity selected_entity)
				{
					while (selected_entity && selected_entity.has_component<hierarchy_component>())
					{
						UUID parent_id = selected_entity.get_component<hierarchy_component>().parent;
						if (parent_id == 0)
							return false;
						if (std::find(selected_ids.begin(), selected_ids.end(), parent_id) != selected_ids.end())
							return true;
						selected_entity = m_active_scene->find_entity_by_UUID(parent_id);
					}
					return false;
				};

			m_scene_hierarchy_panel.clear_selection();
			for (entity selected_entity : selected_entities)
				if (selected_entity && !has_selected_ancestor(selected_entity))
					m_active_scene->destroy_entity(selected_entity);
		}
	}
}

void editor_layer::on_select_all_entities()
{
	if (m_scene_state == scene_state::edit)
		m_scene_hierarchy_panel.select_all();
}

void editor_layer::on_copy_entities()
{
	m_entity_clipboard = m_scene_hierarchy_panel.get_selected_entity_ids();
}

void editor_layer::on_paste_entities()
{
	if (m_scene_state != scene_state::edit || m_entity_clipboard.empty())
		return;

	std::vector<entity> source_entities;
	for (UUID id : m_entity_clipboard)
	{
		entity source = m_editor_scene->find_entity_by_UUID(id);
		if (source)
			source_entities.push_back(source);
	}

	if (source_entities.empty())
		return;

	capture_scene_history();
	bool append = false;
	for (entity source : source_entities)
	{
		entity pasted = m_editor_scene->duplicate_entity(source);
		m_scene_hierarchy_panel.set_selected_entity(pasted, append);
		append = true;
	}
}

void editor_layer::on_cut_entities()
{
	on_copy_entities();
	on_deleted_entity();
}

void editor_layer::UI_toolbar()
{
	bool toolbar_enabled = (bool)m_scene_hierarchy_panel.get_context();

	ImVec4 tint_color = ImVec4(1, 1, 1, 1);
	if (!toolbar_enabled)
		tint_color.w = 0.5f;

	bool has_play_button = m_scene_state == scene_state::edit|| m_scene_state == scene_state::play;
	bool has_simulate_button = m_scene_state == scene_state::edit || m_scene_state == scene_state::simulate;
	bool has_pause_button = m_scene_state != scene_state::edit;
	bool is_paused = has_pause_button && m_active_scene->is_paused();
	bool has_step_button = has_pause_button && is_paused;

	const float button_size = 36.0f;
	const float icon_size = 18.0f;
	const float padding = 6.0f;
	const float spacing = 5.0f;
	const int button_count = (has_play_button ? 1 : 0) + (has_simulate_button ? 1 : 0) + (has_pause_button ? 1 : 0) + (has_step_button ? 1 : 0);
	const float panel_width = padding * 2.0f + button_size * button_count + spacing * glm::max(button_count - 1, 0);
	const float panel_height = button_size + padding * 2.0f;

	ImVec2 viewport_min = ImVec2(m_viewport_bounds[0].x, m_viewport_bounds[0].y);
	ImVec2 viewport_max = ImVec2(m_viewport_bounds[1].x, m_viewport_bounds[1].y);
	ImVec2 panel_pos = ImVec2(viewport_min.x + ((viewport_max.x - viewport_min.x) - panel_width) * 0.5f, viewport_min.y + 12.0f);
	ImVec2 panel_end = ImVec2(panel_pos.x + panel_width, panel_pos.y + panel_height);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(ImVec2(panel_pos.x + 2.0f, panel_pos.y + 3.0f), ImVec2(panel_end.x + 2.0f, panel_end.y + 3.0f), IM_COL32(0, 0, 0, 76), 7.0f);
	draw_list->AddRectFilled(panel_pos, panel_end, IM_COL32(24, 22, 19, 238), 7.0f);
	draw_list->AddRect(panel_pos, panel_end, IM_COL32(76, 64, 48, 210), 7.0f);

	ImGui::SetCursorScreenPos(ImVec2(panel_pos.x + padding, panel_pos.y + padding));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));

	auto draw_icon_button = [&](const char* id, icon icon_type, ImU32 accent, const char* tooltip) -> bool
		{
			ref<texture2D> icon_texture = icon_manager::get().get_icon(icon_type);
			ImGui::InvisibleButton(id, ImVec2(button_size, button_size));
			const bool clicked = ImGui::IsItemClicked() && toolbar_enabled;
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 button_color = active ? color_u32(0.33f, 0.22f, 0.12f, 0.95f) : hovered ? color_u32(0.18f, 0.15f, 0.12f, 0.92f) : color_u32(0.10f, 0.09f, 0.08f, 0.88f);
			draw_list->AddRectFilled(min, max, button_color, 5.0f);
			if (hovered)
				draw_list->AddRect(min, max, accent, 5.0f);

			ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
			ImVec2 icon_min(center.x - icon_size * 0.5f, center.y - icon_size * 0.5f);
			ImVec2 icon_max(center.x + icon_size * 0.5f, center.y + icon_size * 0.5f);
			ImU32 tint = toolbar_enabled ? IM_COL32(240, 232, 216, 255) : IM_COL32(148, 140, 128, 190);
			draw_list->AddImage(UI::to_imgui_texture_id(icon_texture->get_renderer_id()), icon_min, icon_max, ImVec2(0, 1), ImVec2(1, 0), tint);
			if (hovered && tooltip)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		};

	if(has_play_button)
	{
		icon play_icon = m_scene_state == scene_state::play ? icon::stop : icon::play;
		if (draw_icon_button("##ViewportToolbarPlay", play_icon, color_u32(0.58f, 0.70f, 0.42f, tint_color.w), m_scene_state == scene_state::play ? "Stop" : "Play"))
		{
			if (m_scene_state == scene_state::edit || m_scene_state == scene_state::simulate)
				on_scene_play();
			else if (m_scene_state == scene_state::play)
				on_scene_stop();
		}
	}
	if(has_simulate_button)
	{
		if(has_play_button)
			ImGui::SameLine();
		icon simulate_icon = m_scene_state == scene_state::simulate ? icon::stop : icon::simulate;
		if (draw_icon_button("##ViewportToolbarSimulate", simulate_icon, color_u32(0.66f, 0.55f, 0.42f, tint_color.w), m_scene_state == scene_state::simulate ? "Stop simulation" : "Simulate"))
		{
			if (m_scene_state == scene_state::edit || m_scene_state == scene_state::play)
				on_scene_simulate();
			else if (m_scene_state == scene_state::simulate)
				on_scene_stop();
		}
	}
	if (has_pause_button)
	{
		ImGui::SameLine();
		if (draw_icon_button("##ViewportToolbarPause", icon::pause, color_u32(0.86f, 0.64f, 0.32f, tint_color.w), is_paused ? "Resume" : "Pause"))
			m_active_scene->set_paused(!is_paused);

		if (is_paused)
		{
			ImGui::SameLine();
			if (draw_icon_button("##ViewportToolbarStepForward", icon::step_forward, color_u32(0.86f, 0.64f, 0.32f, tint_color.w), "Step"))
				m_active_scene->step(m_UI_settings.get_step_frame());
		}
	}
	ImGui::PopStyleVar();
}

_WHIP_END
