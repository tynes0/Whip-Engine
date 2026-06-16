#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <Whip/Core/Core.h>

_WHIP_START

namespace ScriptProjectGenerator
{
	struct CSharpProjectGenerationSettings
	{
		std::string m_Sdk = "Microsoft.NET.Sdk";
		std::string m_TargetFramework = "net472";
		std::string m_LanguageVersion = "latest";
		std::string m_OutputType = "Library";

		bool m_AppendTargetFrameworkToOutputPath = false;
		bool m_GenerateAssemblyInfo = false;

		std::string m_ScriptCoreProjectName = "Whip-ScriptCore";
		std::string m_ScriptCoreRootNamespace = "Whip";
		std::string m_ScriptCoreOutputPath = "..\\Binaries\\";
		bool m_ScriptCoreAllowUnsafeBlocks = true;

		std::string m_GameOutputPath = "Binaries\\";
		std::string m_ScriptCoreProjectRelativePath = "Whip-ScriptCore\\Whip-ScriptCore.csproj";
		std::string m_ScriptCoreCompileRemovePattern = "Whip-ScriptCore\\**\\*.cs";
		std::vector<std::string> m_GameCompileRemovePatterns =
		{
			"Whip-ScriptCore\\**\\*.cs",
			"Intermediates\\**\\*.cs",
			"obj\\**\\*.cs",
			"Binaries\\**\\*.cs"
		};

		std::string m_IntermediateRoot = "Whip/ScriptIntermediates";
	};

	struct SolutionProjectEntry
	{
		std::string m_Name;
		std::string m_RelativePath;
		std::string m_Guid;
	};

	struct SolutionGenerationSettings
	{
		std::string m_SolutionFormatVersion = "12.00";
		std::string m_VisualStudioCommentVersion = "17";
		std::string m_VisualStudioVersion = "17.0.31903.59";
		std::string m_MinimumVisualStudioVersion = "10.0.40219.1";

		std::string m_CSharpProjectTypeGuid = "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";
		std::vector<std::string> m_Configurations = { "Debug|x64", "Release|x64", "Dist|x64" };

		std::vector<SolutionProjectEntry> m_Projects;
	};

	std::string MakeStableGuid(std::string_view seed);
	std::string MakeScriptCoreCsproj(const CSharpProjectGenerationSettings& settings = CSharpProjectGenerationSettings{});
	std::string MakeProjectCsproj(std::string_view projectFolderName, std::string_view coreGuid, const CSharpProjectGenerationSettings& settings = CSharpProjectGenerationSettings{});
	std::string MakeDirectoryBuildProps(std::string_view projectFolderName, const CSharpProjectGenerationSettings& settings = CSharpProjectGenerationSettings{});
	std::string MakeScriptSolution(const SolutionGenerationSettings& settings);
	std::string MakeStarterScript(std::string_view projectFolderName);
}

_WHIP_END
