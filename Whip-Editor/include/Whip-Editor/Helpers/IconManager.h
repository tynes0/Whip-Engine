#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Texture/TextureManager.h>
#include <Whip/Helper/Buffer.h>
#include <Whip/Utils/Utility.h>

#include <frenum.h>

#include <cstdint>
#include <filesystem>

_WHIP_START

FrenumClassInNamespace(whip, Icon, uint16_t, Directory, File, Back, Play, Stop, Simulate, Pause, StepForward, StepBack)

class IconManager
{
public:
	IconManager(bool loadDefault);
	Ref<Texture2D> Load(Icon iconType, const std::filesystem::path& filepath, FlipDirection direction = FlipDirectionNone);
	Ref<Texture2D> Load(Icon iconType, Ref<Texture2D> texture);
	Ref<Texture2D> GetIcon(Icon iconType);

	static IconManager& Get();
private:
	bool ValidIcon(Icon iconType);

	StackBuffer<frenum::size<Icon>() * sizeof(Ref<Texture2D>), 16> m_IconDatas;
};

_WHIP_END
