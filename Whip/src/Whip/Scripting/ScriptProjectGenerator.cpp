#include <Whip/Scripting/ScriptProjectGenerator.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

_WHIP_START

namespace ScriptProjectGenerator
{
	namespace
	{
		const char* BoolToMsbuild(bool value)
		{
			return value ? "true" : "false";
		}

		uint64_t Fnv1a64(std::string_view value)
		{
			uint64_t hash = 14695981039346656037ull;
			for (char c : value)
			{
				hash ^= static_cast<unsigned char>(c);
				hash *= 1099511628211ull;
			}
			return hash;
		}
	}

	std::string MakeStableGuid(std::string_view seed)
	{
		uint64_t first = Fnv1a64(seed);

		std::string secondSeed(seed);
		secondSeed += ":whip";

		uint64_t second = Fnv1a64(secondSeed);

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

	std::string MakeScriptCoreCsproj(const CSharpProjectGenerationSettings& settings)
	{
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project Sdk=\"" << settings.m_Sdk << "\">\n"
			<< "  <PropertyGroup>\n"
			<< "    <TargetFramework>" << settings.m_TargetFramework << "</TargetFramework>\n"
			<< "    <RootNamespace>" << settings.m_ScriptCoreRootNamespace << "</RootNamespace>\n"
			<< "    <AssemblyName>" << settings.m_ScriptCoreProjectName << "</AssemblyName>\n"
			<< "    <OutputType>" << settings.m_OutputType << "</OutputType>\n"
			<< "    <OutputPath>" << settings.m_ScriptCoreOutputPath << "</OutputPath>\n"
			<< "    <AppendTargetFrameworkToOutputPath>" << BoolToMsbuild(settings.m_AppendTargetFrameworkToOutputPath) << "</AppendTargetFrameworkToOutputPath>\n"
			<< "    <AllowUnsafeBlocks>" << BoolToMsbuild(settings.m_ScriptCoreAllowUnsafeBlocks) << "</AllowUnsafeBlocks>\n"
			<< "    <LangVersion>" << settings.m_LanguageVersion << "</LangVersion>\n"
			<< "    <GenerateAssemblyInfo>" << BoolToMsbuild(settings.m_GenerateAssemblyInfo) << "</GenerateAssemblyInfo>\n"
			<< "  </PropertyGroup>\n"
			<< "</Project>\n";

		return stream.str();
	}

	std::string MakeProjectCsproj(
		std::string_view projectFolderName,
		std::string_view coreGuid,
		const CSharpProjectGenerationSettings& settings)
	{
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project Sdk=\"" << settings.m_Sdk << "\">\n"
			<< "  <PropertyGroup>\n"
			<< "    <TargetFramework>" << settings.m_TargetFramework << "</TargetFramework>\n"
			<< "    <RootNamespace>" << projectFolderName << "</RootNamespace>\n"
			<< "    <AssemblyName>" << projectFolderName << "</AssemblyName>\n"
			<< "    <OutputType>" << settings.m_OutputType << "</OutputType>\n"
			<< "    <OutputPath>" << settings.m_GameOutputPath << "</OutputPath>\n"
			<< "    <AppendTargetFrameworkToOutputPath>" << BoolToMsbuild(settings.m_AppendTargetFrameworkToOutputPath) << "</AppendTargetFrameworkToOutputPath>\n"
			<< "    <LangVersion>" << settings.m_LanguageVersion << "</LangVersion>\n"
			<< "    <GenerateAssemblyInfo>" << BoolToMsbuild(settings.m_GenerateAssemblyInfo) << "</GenerateAssemblyInfo>\n"
			<< "  </PropertyGroup>\n"
			<< "  <ItemGroup>\n";

		for (const std::string& pattern : settings.m_GameCompileRemovePatterns)
			stream << "    <Compile Remove=\"" << pattern << "\" />\n";

		stream
			<< "  </ItemGroup>\n"
			<< "  <ItemGroup>\n"
			<< "    <ProjectReference Include=\"" << settings.m_ScriptCoreProjectRelativePath << "\">\n"
			<< "      <Project>" << coreGuid << "</Project>\n"
			<< "      <Name>" << settings.m_ScriptCoreProjectName << "</Name>\n"
			<< "      <Private>False</Private>\n"
			<< "    </ProjectReference>\n"
			<< "  </ItemGroup>\n"
			<< "</Project>\n";

		return stream.str();
	}

	std::string MakeDirectoryBuildProps(
		std::string_view projectFolderName,
		const CSharpProjectGenerationSettings& settings)
	{
		std::ostringstream stream;
		stream
			<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			<< "<Project>\n"
			<< "  <PropertyGroup>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(TEMP)' != ''\">$(TEMP)/" << settings.m_IntermediateRoot << "/" << projectFolderName << "/</WhipScriptIntermediateRoot>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(WhipScriptIntermediateRoot)' == '' and '$(TMPDIR)' != ''\">$(TMPDIR)/" << settings.m_IntermediateRoot << "/" << projectFolderName << "/</WhipScriptIntermediateRoot>\n"
			<< "    <WhipScriptIntermediateRoot Condition=\"'$(WhipScriptIntermediateRoot)' == ''\">$(MSBuildThisFileDirectory)Intermediates/" << projectFolderName << "/</WhipScriptIntermediateRoot>\n"
			<< "    <BaseIntermediateOutputPath>$(WhipScriptIntermediateRoot)$(MSBuildProjectName)/</BaseIntermediateOutputPath>\n"
			<< "    <MSBuildProjectExtensionsPath>$(BaseIntermediateOutputPath)</MSBuildProjectExtensionsPath>\n"
			<< "  </PropertyGroup>\n"
			<< "</Project>\n";

		return stream.str();
	}

	std::string MakeScriptSolution(const SolutionGenerationSettings& settings)
	{
		std::ostringstream stream;
		stream
			<< "Microsoft Visual Studio Solution File, Format Version " << settings.m_SolutionFormatVersion << "\n"
			<< "# Visual Studio Version " << settings.m_VisualStudioCommentVersion << "\n"
			<< "VisualStudioVersion = " << settings.m_VisualStudioVersion << "\n"
			<< "MinimumVisualStudioVersion = " << settings.m_MinimumVisualStudioVersion << "\n";

		for (const SolutionProjectEntry& project : settings.m_Projects)
		{
			stream
				<< "Project(\"" << settings.m_CSharpProjectTypeGuid << "\") = \""
				<< project.m_Name << "\", \""
				<< project.m_RelativePath << "\", \""
				<< project.m_Guid << "\"\n"
				<< "EndProject\n";
		}

		stream
			<< "Global\n"
			<< "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";

		for (const std::string& config : settings.m_Configurations)
			stream << "\t\t" << config << " = " << config << "\n";

		stream
			<< "\tEndGlobalSection\n"
			<< "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";

		for (const SolutionProjectEntry& project : settings.m_Projects)
		{
			for (const std::string& config : settings.m_Configurations)
			{
				stream << "\t\t" << project.m_Guid << "." << config << ".ActiveCfg = " << config << "\n";
				stream << "\t\t" << project.m_Guid << "." << config << ".Build.0 = " << config << "\n";
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

	std::string MakeStarterScript(std::string_view projectFolderName)
	{
		std::ostringstream stream;
		stream
			<< "using Whip;\n\n"
			<< "namespace " << projectFolderName << "\n"
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
}

_WHIP_END
