#pragma once

#include <Whip/Core/Core.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

_WHIP_START

namespace FileExtensions
{
	inline constexpr const char* Project = ".wproj";
	inline constexpr const char* Scene = ".wscene";
	inline constexpr const char* SceneLegacy = ".whip";
	inline constexpr const char* AssetRegistry = ".wreg";
	inline constexpr const char* AssetRegistryLegacy = ".whipr";
	inline constexpr const char* Animation = ".wanim";
	inline constexpr const char* EntityTemplate = ".went";

	inline constexpr const char* AssetRegistryFilename = "AssetRegistry.wreg";
	inline constexpr const char* AssetRegistryLegacyFilename = "AssetRegistry.whipr";

	inline bool EqualsIgnoreCase(std::string lhs, std::string rhs)
	{
		if (lhs.size() != rhs.size())
			return false;

		return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
			[](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
	}

	inline bool ExtensionEquals(const std::filesystem::path& path, const char* extension)
	{
		return EqualsIgnoreCase(path.extension().string(), extension);
	}

	inline bool IsProjectExtension(const std::filesystem::path& path)
	{
		return ExtensionEquals(path, Project);
	}

	inline bool IsSceneExtension(const std::filesystem::path& path)
	{
		return ExtensionEquals(path, Scene) || ExtensionEquals(path, SceneLegacy);
	}

	inline bool IsLegacySceneExtension(const std::filesystem::path& path)
	{
		return ExtensionEquals(path, SceneLegacy);
	}

	inline bool IsEntityTemplateExtension(const std::filesystem::path& path)
	{
		return ExtensionEquals(path, EntityTemplate);
	}

	inline bool IsAssetRegistryFilename(const std::filesystem::path& path)
	{
		const std::string filename = path.filename().string();
		return EqualsIgnoreCase(filename, AssetRegistryFilename) || EqualsIgnoreCase(filename, AssetRegistryLegacyFilename);
	}
}

_WHIP_END
