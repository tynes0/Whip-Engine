#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Timestep.h>

#include <vector>

#include <imgui.h>

_WHIP_START

namespace UI
{
	struct UIImageData
	{
		ImTextureID m_TextureId;
		ImVec2 m_Size;
		ImVec2 m_Uv1;
		ImVec2 m_Uv2;
	};

	class UIStatistics
	{
	public:
		UIStatistics() = default;

		void AddImage(ImTextureID textureId, const ImVec2& size, const ImVec2& uv1, const ImVec2& uv2);

		void SetOpen(bool open);
		bool IsOpen() const { return m_Open; }
		bool ConsumeOpenDirty();
		void OnImGuiRender(Timestep ts);
	private:
		std::vector<UIImageData> m_ImageData;
		bool m_Open = true;
		bool m_OpenDirty = false;
	};
}

_WHIP_END
