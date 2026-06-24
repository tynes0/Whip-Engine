#include <Whip-Editor/Panels/AssetEditorPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Asset/TextureSlicer.h>
#include <Whip/Audio/AudioEngine.h>
#include <Whip/Audio/AudioSource.h>
#include <Whip/Render/Font.h>
#include <Whip/Render/MsdfData.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <deque>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

_WHIP_START

namespace
{
	enum class WindowControlType : uint8_t
	{
		Minimize,
		Maximize,
		Restore
	};

	const char* AssetTypeName(AssetType type)
	{
		switch (type)
		{
		case AssetType::Scene: return "Scene";
		case AssetType::Texture2D: return "Texture";
		case AssetType::Audio: return "Audio";
		case AssetType::Font: return "Font";
		case AssetType::Animation: return "Animation";
		case AssetType::AnimationController: return "Animation Controller";
		case AssetType::Entity: return "Entity Template";
		case AssetType::None: return "Asset";
		}
		return "Asset";
	}

	std::string FormatFileSize(const std::filesystem::path& path)
	{
		std::error_code error;
		if (!std::filesystem::exists(path, error))
			return "Missing";

		const std::uintmax_t size = std::filesystem::file_size(path, error);
		if (error)
			return "Unknown";

		if (size < 1024)
			return std::to_string(size) + " B";
		if (size < static_cast<uintmax_t>(1024) * 1024)
			return std::to_string(size / 1024) + " KB";

		char buffer[32]{};
		int result = std::snprintf(buffer, sizeof(buffer), "%.2f MB", static_cast<double>(size) / (1024.0 * 1024.0));

		if (result < 0 || std::cmp_greater_equal(result, sizeof(buffer)))
			WHP_EDITOR_WARN("[Asset Editor] Buffer writing failed!");
		return buffer;
	}

	std::string FormatDuration(float seconds)
	{
		const int totalSeconds = static_cast<int>(std::max(seconds, 0.0f));
		const int minutes = totalSeconds / 60;
		const int remainingSeconds = totalSeconds % 60;
		char buffer[32]{};
		int result = std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, remainingSeconds);
		if (result < 0 || std::cmp_equal(result, sizeof(buffer)))
			WHP_EDITOR_WARN("[Asset Editor] Buffer writing failed!");
		return buffer;
	}

	ImVec2 FitImageSize(float width, float height, ImVec2 available)
	{
		if (width <= 0.0f || height <= 0.0f)
			return { 96.0f, 96.0f };

		available.x = std::max(96.0f, available.x);
		available.y = std::max(96.0f, available.y);
		const float scale = std::min(available.x / width, available.y / height);
		return { width * scale, height * scale };
	}

	ImVec2 ClampDefaultWindowSize(ImVec2 requested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 maxSize(
			std::min(1280.0f, viewport->WorkSize.x - 40.0f),
			std::min(720.0f, viewport->WorkSize.y - 40.0f));
		return {
			std::clamp(requested.x, 360.0f, std::max(360.0f, maxSize.x)),
			std::clamp(requested.y, 240.0f, std::max(240.0f, maxSize.y))
		};
	}

	ImVec2 DefaultWorkspaceSize()
	{
		return ClampDefaultWindowSize({ 1040.0f, 640.0f });
	}

	bool DrawWindowControl(const char* id, WindowControlType type)
	{
		constexpr ImVec2 size(28.0f, 22.0f);
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 hoverColor = IM_COL32(255, 255, 255, hovered ? 24 : 0);
		drawList->AddRectFilled(min, max, hoverColor, 3.0f);
		constexpr ImU32 color = IM_COL32(226, 226, 226, 230);
		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

		if (type == WindowControlType::Minimize)
		{
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 4.0f), ImVec2(center.x + 5.0f, center.y + 4.0f), color, 1.4f);
		}
		else if (type == WindowControlType::Maximize)
		{
			drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), color, 0.0f, 0, 1.2f);
		}
		else
		{
			drawList->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 7.0f, center.y + 4.0f), color, 0.0f, 0, 1.1f);
			drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 2.0f), ImVec2(center.x + 3.0f, center.y + 8.0f), color, 0.0f, 0, 1.1f);
		}

		return clicked;
	}

	bool TitlebarDragStarted()
	{
		const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
		if (!window || !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
			return false;

		const ImVec2 click = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
		const float titlebarBottom = window->Pos.y + ImGui::GetFrameHeight();
		return click.y >= window->Pos.y && click.y <= titlebarBottom;
	}

	uint32_t BytesPerPixel(ImageFormat format)
	{
		switch (format) // NOLINT(clang-diagnostic-switch-enum)
		{
		case ImageFormat::Rgb8: return 3;
		case ImageFormat::Rgba8: return 4;
		default: return 0;
		}
	}

	const char* ImageFormatName(ImageFormat format)
	{
		switch (format) // NOLINT(clang-diagnostic-switch-enum)
		{
		case ImageFormat::R8: return "R8";
		case ImageFormat::Rgb8: return "RGB8";
		case ImageFormat::Rgba8: return "RGBA8";
		case ImageFormat::Rgba32F: return "RGBA32F";
		default: return "None";
		}
	}

	const char* TextureFilterModeName(TextureFilterMode mode)
	{
		switch (mode)
		{
		case TextureFilterMode::Nearest: return "Nearest";
		case TextureFilterMode::Linear: return "Linear";
		}
		return "Linear";
	}

	const char* TextureWrapModeName(TextureWrapMode mode)
	{
		switch (mode)
		{
		case TextureWrapMode::ClampToEdge: return "Clamp To Edge";
		case TextureWrapMode::Repeat: return "Repeat";
		}
		return "Repeat";
	}

	const char* TextureSpriteModeName(TextureSpriteMode mode)
	{
		switch (mode)
		{
		case TextureSpriteMode::Multiple: return "Multiple";
		case TextureSpriteMode::Single: return "Single";
		}

		return "Single";
	}

	const char* TextureEditorToolName(AssetEditorPanel::TextureEditorTool tool)
	{
		switch (tool)
		{
		case AssetEditorPanel::TextureEditorTool::Brush: return "Brush";
		case AssetEditorPanel::TextureEditorTool::Eraser: return "Eraser";
		case AssetEditorPanel::TextureEditorTool::Picker: return "Picker";
		case AssetEditorPanel::TextureEditorTool::Fill: return "Fill";
		case AssetEditorPanel::TextureEditorTool::Slice: return "Slice";
		}
		return "Tool";
	}

	const char* TextureEditorToolHint(AssetEditorPanel::TextureEditorTool tool)
	{
		switch (tool)
		{
		case AssetEditorPanel::TextureEditorTool::Brush: return "paint pixels";
		case AssetEditorPanel::TextureEditorTool::Eraser: return "erase to transparent";
		case AssetEditorPanel::TextureEditorTool::Picker: return "pick color";
		case AssetEditorPanel::TextureEditorTool::Fill: return "fill region";
		case AssetEditorPanel::TextureEditorTool::Slice: return "draw sprite rect";
		}
		return "";
	}

	ImU32 TextureEditorToolColor(AssetEditorPanel::TextureEditorTool tool)
	{
		switch (tool)
		{
		case AssetEditorPanel::TextureEditorTool::Brush: return IM_COL32(116, 186, 238, 235);
		case AssetEditorPanel::TextureEditorTool::Eraser: return IM_COL32(236, 132, 126, 235);
		case AssetEditorPanel::TextureEditorTool::Picker: return IM_COL32(108, 206, 181, 235);
		case AssetEditorPanel::TextureEditorTool::Fill: return IM_COL32(242, 190, 96, 235);
		case AssetEditorPanel::TextureEditorTool::Slice: return IM_COL32(184, 145, 238, 235);
		}
		return IM_COL32(180, 190, 200, 235);
	}

	float TextureInspectorItemWidth()
	{
		const float available = ImGui::GetContentRegionAvail().x;
		const float labelReserve = std::clamp(available * 0.40f, 104.0f, 148.0f);
		return std::max(72.0f, available - labelReserve);
	}

	float Median(float a, float b, float c)
	{
		return std::max(std::min(a, b), std::min(std::max(a, b), c));
	}

	Ref<Texture2D> BuildFontPreviewTexture(const Ref<Font>& font, const std::string& text, float scale, uint32_t width, uint32_t height)
	{
		if (!font || !font->GetMsdfData() || !font->GetAtlasTexture() || width == 0 || height == 0)
			return nullptr;

		const auto& fontGeometry = font->GetMsdfData()->m_FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		Ref<Texture2D> atlas = font->GetAtlasTexture();
		if (Math::EqualF(metrics.ascenderY, metrics.descenderY) || atlas->GetWidth() == 0 || atlas->GetHeight() == 0)
			return nullptr;

		RawBuffer atlasData = atlas->GetData();
		if (!atlasData)
			return nullptr;

		const double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		const float pixelScale = std::max(18.0f, 42.0f * scale);
		const float lineAdvance = static_cast<float>(fsScale * metrics.lineHeight) * pixelScale + 8.0f * scale;
		const auto spaceGlyph = fontGeometry.getGlyph(' ');
		const float spaceAdvance = (spaceGlyph ? static_cast<float>(spaceGlyph->getAdvance()) : 1.0f) * static_cast<float>(fsScale) * pixelScale;
		float x = 16.0f;
		float baselineY = 24.0f + static_cast<float>(metrics.ascenderY * fsScale) * pixelScale;
		const uint32_t atlasWidth = atlas->GetWidth();
		const uint32_t atlasHeight = atlas->GetHeight();
		std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);

		for (size_t index = 0; index < text.size(); ++index)
		{
			const char character = text[index];
			if (character == '\r')
				continue;
			if (character == '\n')
			{
				x = 16.0f;
				baselineY += lineAdvance;
				continue;
			}
			if (character == ' ')
			{
				x += spaceAdvance;
				continue;
			}
			if (character == '\t')
			{
				x += spaceAdvance * 4.0f;
				continue;
			}

			auto glyph = fontGeometry.getGlyph(static_cast<unsigned char>(character));
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');
			if (!glyph)
				continue;

			double al, ab, ar, at;
			glyph->getQuadAtlasBounds(al, ab, ar, at);
			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);

			const ImVec2 glyphMin(
				x + static_cast<float>(pl * fsScale) * pixelScale,
				baselineY - static_cast<float>(pt * fsScale) * pixelScale);
			const ImVec2 glyphMax(
				x + static_cast<float>(pr * fsScale) * pixelScale,
				baselineY - static_cast<float>(pb * fsScale) * pixelScale);
			const float glyphWidth = glyphMax.x - glyphMin.x;
			const float glyphHeight = glyphMax.y - glyphMin.y;
			const float atlasGlyphWidth = static_cast<float>(ar - al);
			const float atlasGlyphHeight = static_cast<float>(at - ab);
			if (glyphWidth > 0.0f && glyphHeight > 0.0f && atlasGlyphWidth > 0.0f && atlasGlyphHeight > 0.0f)
			{
				const int minX = std::clamp(static_cast<int>(std::floor(glyphMin.x)), 0, static_cast<int>(width));
				const int minY = std::clamp(static_cast<int>(std::floor(glyphMin.y)), 0, static_cast<int>(height));
				const int maxX = std::clamp(static_cast<int>(std::ceil(glyphMax.x)), 0, static_cast<int>(width));
				const int maxY = std::clamp(static_cast<int>(std::ceil(glyphMax.y)), 0, static_cast<int>(height));
				const float screenPxRange = std::max(1.0f, glyphWidth / atlasGlyphWidth + glyphHeight / atlasGlyphHeight);

				for (int py = minY; py < maxY; ++py)
				{
					const float vertical = (static_cast<float>(py) + 0.5f - glyphMin.y) / glyphHeight;
					const float sampleY = static_cast<float>(at) + vertical * static_cast<float>(ab - at);
					const int atlasY = std::clamp(static_cast<int>(std::floor(sampleY)), 0, static_cast<int>(atlasHeight) - 1);
					for (int px = minX; px < maxX; ++px)
					{
						const float horizontal = (static_cast<float>(px) + 0.5f - glyphMin.x) / glyphWidth;
						const float sampleX = static_cast<float>(al) + horizontal * static_cast<float>(ar - al);
						const int atlasX = std::clamp(static_cast<int>(std::floor(sampleX)), 0, static_cast<int>(atlasWidth) - 1);
						const uint8_t* source = atlasData.m_Data + (static_cast<uint64_t>(atlasY) * atlasWidth + atlasX) * 3;
						const float signedDistance = Median(
							static_cast<float>(source[0]) / 255.0f,
							static_cast<float>(source[1]) / 255.0f,
							static_cast<float>(source[2]) / 255.0f);
						const float opacity = std::clamp(screenPxRange * (signedDistance - 0.5f) + 0.5f, 0.0f, 1.0f);
						if (opacity <= 0.0f)
							continue;

						const uint32_t textureY = height - 1 - static_cast<uint32_t>(py);
						uint8_t* destination = pixels.data() + (static_cast<uint64_t>(textureY) * width + static_cast<uint32_t>(px)) * 4;
						const uint8_t alpha = static_cast<uint8_t>(std::lround(opacity * 255.0f + 0.5f));
						destination[0] = 235;
						destination[1] = 242;
						destination[2] = 248;
						destination[3] = std::max(destination[3], alpha);
					}
				}
			}

			double advance = glyph->getAdvance();
			if (index + 1 < text.size())
				fontGeometry.getAdvance(advance, character, text[index + 1]);
			x += static_cast<float>(advance * fsScale) * pixelScale;
		}

		atlasData.Release();

		TextureSpecification specification;
		specification.m_Width = width;
		specification.m_Height = height;
		specification.m_Format = ImageFormat::Rgba8;
		specification.m_GenerateMips = false;
		specification.m_FilterMode = TextureFilterMode::Linear;
		specification.m_WrapMode = TextureWrapMode::ClampToEdge;
		return Texture2D::Create(specification, RawBuffer(pixels.data(), static_cast<uint64_t>(pixels.size())));
	}

	uint8_t ColorFloatToByte(float value)
	{
		return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);  // NOLINT(bugprone-incorrect-roundings)
	}

	std::array<uint8_t, 4> BrushColorBytes(const std::array<float, 4>& color)
	{
		return {
			ColorFloatToByte(color[0]),
			ColorFloatToByte(color[1]),
			ColorFloatToByte(color[2]),
			ColorFloatToByte(color[3])
		};
	}

	std::string LowercaseExtension(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::ranges::transform(extension, extension.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return extension;
	}

	void WriteBigEndianU32(std::vector<uint8_t>& buffer, uint32_t value)
	{
		buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
		buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
		buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
		buffer.push_back(static_cast<uint8_t>(value & 0xFF));
	}

	uint32_t Crc32(const uint8_t* data, size_t size)
	{
		static uint32_t table[256]{};
		static bool initialized = false;
		if (!initialized)
		{
			for (uint32_t i = 0; i < 256; ++i)
			{
				uint32_t crc = i;
				for (int bit = 0; bit < 8; ++bit)
					crc = (crc & 1U) ? (0xEDB88320U ^ (crc >> 1U)) : (crc >> 1U);
				table[i] = crc;
			}
			initialized = true;
		}

		uint32_t crc = 0xFFFFFFFFU;
		for (size_t i = 0; i < size; ++i)
			crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8U);
		return crc ^ 0xFFFFFFFFU;
	}

	uint32_t Adler32(const std::vector<uint8_t>& data)
	{
		uint32_t a = 1;
		uint32_t b = 0;
		for (uint8_t byte : data)
		{
			constexpr uint32_t Mod = 65521;
			a = (a + byte) % Mod;
			b = (b + a) % Mod;
		}
		return (b << 16U) | a;
	}

	void WritePngChunk(std::vector<uint8_t>& png, const char type[4], const std::vector<uint8_t>& data)
	{
		WriteBigEndianU32(png, static_cast<uint32_t>(data.size()));
		const size_t crcStart = png.size();
		png.insert(png.end(), type, type + 4);
		png.insert(png.end(), data.begin(), data.end());
		const uint32_t crc = Crc32(png.data() + crcStart, png.size() - crcStart);
		WriteBigEndianU32(png, crc);
	}

	bool WritePngFile(const std::filesystem::path& path, const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height, uint32_t channels, std::string& error)
	{
		if (width == 0 || height == 0 || (channels != 3 && channels != 4))
		{
			error = "Texture format is not editable.";
			return false;
		}

		const uint64_t expectedSize = static_cast<uint64_t>(width) * height * channels;
		if (pixels.size() != expectedSize)
		{
			error = "Texture edit buffer is out of sync.";
			return false;
		}

		std::vector<uint8_t> scanlines;
		scanlines.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 4 + 1));
		for (uint32_t y = 0; y < height; ++y)
		{
			scanlines.push_back(0);
			const uint32_t storageY = height - 1 - y;
			for (uint32_t x = 0; x < width; ++x)
			{
				const size_t index = (static_cast<size_t>(storageY) * width + x) * channels;
				scanlines.push_back(pixels[index + 0]);
				scanlines.push_back(pixels[index + 1]);
				scanlines.push_back(pixels[index + 2]);
				scanlines.push_back(channels == 4 ? pixels[index + 3] : 255);
			}
		}

		std::vector<uint8_t> zlib;
		zlib.reserve(scanlines.size() + scanlines.size() / 65535 * 5 + 16);
		zlib.push_back(0x78);
		zlib.push_back(0x01);

		size_t offset = 0;
		while (offset < scanlines.size())
		{
			const uint16_t blockSize = static_cast<uint16_t>(std::min<size_t>(65535, scanlines.size() - offset));
			const bool finalBlock = offset + blockSize >= scanlines.size();
			zlib.push_back(finalBlock ? 0x01 : 0x00);
			zlib.push_back(static_cast<uint8_t>(blockSize & 0xFF));
			zlib.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
			const uint16_t inverse = static_cast<uint16_t>(~blockSize);
			zlib.push_back(static_cast<uint8_t>(inverse & 0xFF));
			zlib.push_back(static_cast<uint8_t>((inverse >> 8) & 0xFF));
			zlib.insert(zlib.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(offset), scanlines.begin() + static_cast<std::ptrdiff_t>(offset + blockSize));
			offset += blockSize;
		}

		WriteBigEndianU32(zlib, Adler32(scanlines));

		std::vector<uint8_t> png = { 137, 80, 78, 71, 13, 10, 26, 10 };
		std::vector<uint8_t> ihdr;
		ihdr.reserve(13);
		WriteBigEndianU32(ihdr, width);
		WriteBigEndianU32(ihdr, height);
		ihdr.push_back(8);
		ihdr.push_back(6);
		ihdr.push_back(0);
		ihdr.push_back(0);
		ihdr.push_back(0);
		WritePngChunk(png, "IHDR", ihdr);
		WritePngChunk(png, "IDAT", zlib);
		WritePngChunk(png, "IEND", {});

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			error = "Could not open texture file for writing.";
			return false;
		}

		output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
		if (!output)
		{
			error = "Could not finish writing PNG.";
			return false;
		}

		return true;
	}

	std::string ReadTextPreview(const std::filesystem::path& path, size_t maxBytes = 24000)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
			return {};

		std::string result;
		result.resize(maxBytes);
		input.read(result.data(), static_cast<std::streamsize>(result.size()));
		result.resize(static_cast<size_t>(input.gcount()));
		if (!input.eof())
			result += "\n...";
		return result;
	}
}

AssetEditorPanel::AssetEditorPanel()
	: EditorPanel("Asset Workspace", false)
{
}

void AssetEditorPanel::OpenAsset(AssetHandle handle)
{
	if (handle == 0 || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return;

	for (AssetEditorDocument& document : m_Documents)
		document.m_FocusRequested = false;

	for (AssetEditorDocument& document : m_Documents)
	{
		if (document.m_Handle == handle)
		{
			document.m_Open = true;
			document.m_FocusRequested = true;
			m_Open = true;
			if (m_Minimized)
				MarkLayoutDirty();
			m_Minimized = false;
			m_ActiveDocument = handle;
			m_FocusRequested = true;
			m_OpenDirty = true;
			return;
		}
	}

	AssetEditorDocument document;
	document.m_Handle = handle;
	m_Documents.push_back(document);
	m_Open = true;
	if (m_Minimized)
		MarkLayoutDirty();
	m_Minimized = false;
	m_ActiveDocument = handle;
	m_FocusRequested = true;
	m_OpenDirty = true;
}

void AssetEditorPanel::CloseAll()
{
	if (m_Documents.empty())
		return;

	m_Documents.clear();
	m_Open = false;
	m_Minimized = false;
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	m_ActiveDocument = 0;
	m_EmbeddedAnimationHandle = 0;
	m_OpenDirty = true;
	MarkLayoutDirty();
}

void AssetEditorPanel::RegisterShortcuts(EditorShortcutManager& shortcuts)
{
	auto add = [this, &shortcuts](const char* id, const char* displayName, const char* category, const UI::ShortcutBinding& binding, std::function<bool()> callback, std::function<bool()> isAvailable = {}, EditorShortcutOptions options = {})
	{
		shortcuts.Add(
			EditorShortcutScope::AssetEditor,
			std::string("asset_editor.") + id,
			displayName,
			category,
			binding,
			std::move(callback),
			std::move(isAvailable),
			[this]() { return IsShortcutContextActive(); },
			options);
	};

	EditorShortcutOptions panelOptions;
	panelOptions.m_AllowWhenActiveWidget = true;

	add("next_tab", "Next Asset Editor Tab", "Workspace", { Key::Tab, true, false, false }, [this]() { FocusNextEditor(); return true; }, [this]() { return m_Documents.size() > 1; }, panelOptions);
	add("close_tab", "Close Active Asset Editor Tab", "Workspace", { Key::W, true, false, false }, [this]() { return CloseActiveEditor(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("minimize", "Minimize Asset Workspace", "Workspace", { Key::M, true, true, false }, [this]() { return MinimizeWorkspace(); }, {}, panelOptions);
	add("fullscreen", "Toggle Asset Workspace Fullscreen", "Workspace", { Key::F11, false, false, false }, [this]() { return ToggleFullscreenWorkspace(); }, {}, panelOptions);
	add("texture_brush", "Texture Brush Tool", "Texture Tools", { Key::B, false, false, false }, [this]() { return SetActiveTextureTool(TextureEditorTool::Brush); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_eraser", "Texture Eraser Tool", "Texture Tools", { Key::E, false, false, false }, [this]() { return SetActiveTextureTool(TextureEditorTool::Eraser); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_picker", "Texture Picker Tool", "Texture Tools", { Key::P, false, false, false }, [this]() { return SetActiveTextureTool(TextureEditorTool::Picker); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_fill", "Texture Fill Tool", "Texture Tools", { Key::F, false, false, false }, [this]() { return SetActiveTextureTool(TextureEditorTool::Fill); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_slice", "Texture Slice Tool", "Texture Tools", { Key::S, false, false, false }, [this]() { return SetActiveTextureTool(TextureEditorTool::Slice); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_undo", "Undo Texture Edit", "Texture Edit", { Key::Z, true, false, false }, [this]() { return UndoActiveTextureEdit(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_redo", "Redo Texture Edit", "Texture Edit", { Key::Y, true, false, false }, [this]() { return RedoActiveTextureEdit(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_save", "Save Active Texture PNG", "Texture File", { Key::S, true, false, false }, [this]() { return SaveActiveTexture(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_apply", "Apply Texture Preview", "Texture File", { Key::Enter, true, false, false }, [this]() { return ApplyActiveTexturePreview(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_reload", "Reload Active Texture", "Texture File", { Key::R, true, false, false }, [this]() { return ReloadActiveTexture(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_reset_view", "Reset Texture View", "Texture View", { Key::D0, true, false, false }, [this]() { return ResetActiveTextureView(); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_zoom_in", "Texture Zoom In", "Texture View", { Key::Equal, true, false, false }, [this]() { return ZoomActiveTexture(1.2f); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
	add("texture_zoom_out", "Texture Zoom Out", "Texture View", { Key::Minus, true, false, false }, [this]() { return ZoomActiveTexture(1.0f / 1.2f); }, [this]() { return GetActiveDocument() != nullptr; }, panelOptions);
}

void AssetEditorPanel::SetOpen(bool open)
{
	if (open)
	{
		if (m_Documents.empty())
			return;

		EditorPanel::SetOpen(true);
		if (m_Minimized)
			MarkLayoutDirty();
		m_Minimized = false;
		m_FocusRequested = true;
		return;
	}

	CloseAll();
}

bool AssetEditorPanel::IsOpen() const
{
	return m_Open;
}

bool AssetEditorPanel::CanOpenFromMenu() const
{
	return HasOpenEditors();
}

void AssetEditorPanel::SetOpenSceneCallback(std::function<void(AssetHandle)> callback)
{
	m_OpenSceneCallback = std::move(callback);
}

void AssetEditorPanel::SetSetStartSceneCallback(std::function<void(AssetHandle)> callback)
{
	m_SetStartSceneCallback = std::move(callback);
}

void AssetEditorPanel::SetOpenAnimationCallback(std::function<bool(AssetHandle)> callback)
{
	m_OpenAnimationCallback = std::move(callback);
}

void AssetEditorPanel::SetDrawAnimationEditorCallback(std::function<void()> callback)
{
	m_DrawAnimationEditorCallback = std::move(callback);
}

void AssetEditorPanel::SetRefreshAssetTreeCallback(std::function<void()> callback)
{
	m_RefreshAssetTreeCallback = std::move(callback);
}

bool AssetEditorPanel::HasOpenEditors() const
{
	return !m_Documents.empty();
}

bool AssetEditorPanel::ConsumeOpenDirty()
{
	const bool dirty = m_OpenDirty;
	m_OpenDirty = false;
	return dirty;
}

bool AssetEditorPanel::ConsumeLayoutDirty()
{
	const bool dirty = m_LayoutDirty;
	m_LayoutDirty = false;
	return dirty;
}

AssetEditorPanel::WorkspacePreferences AssetEditorPanel::GetWorkspacePreferences() const
{
	WorkspacePreferences preferences;
	preferences.m_Open = m_Open;
	preferences.m_Minimized = m_Minimized;
	preferences.m_Fullscreen = m_Fullscreen;
	preferences.m_HasRestoreRect = m_HasRestoreRect;
	preferences.m_RestorePosition = m_RestorePosition;
	preferences.m_RestoreSize = m_RestoreSize;
	return preferences;
}

void AssetEditorPanel::ApplyWorkspacePreferences(const WorkspacePreferences& preferences)
{
	m_Minimized = preferences.m_Minimized;
	m_Fullscreen = preferences.m_Fullscreen;
	m_FullscreenRequested = preferences.m_Fullscreen;
	m_HasRestoreRect = preferences.m_HasRestoreRect;
	m_RestorePosition = preferences.m_RestorePosition;
	m_RestoreSize = preferences.m_RestoreSize;
	if (preferences.m_Open && !m_Documents.empty())
	{
		m_Open = true;
		m_FocusRequested = true;
	}
	m_LayoutDirty = false;
}

void AssetEditorPanel::OnImGuiRender()
{
	if (m_Documents.empty())
	{
		m_Open = false;
		m_ShortcutContextActive = false;
		return;
	}

	if (!m_Open)
	{
		m_ShortcutContextActive = false;
		return;
	}

	HandleWorkspaceTabShortcut();

	if (m_Minimized)
	{
		DrawMinimizedStrip();
		return;
	}

	if (m_FullscreenRequested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
		m_FullscreenRequested = false;
	}
	else if (m_Fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	}

	if (m_FocusRequested)
	{
		ImGui::SetNextWindowFocus();
		m_FocusRequested = false;
	}

	if (!m_Fullscreen)
		ImGui::SetNextWindowSize(DefaultWorkspaceSize(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(340.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
	bool open = m_Open;
	constexpr ImGuiWindowFlags WorkspaceFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (!ImGui::Begin("Asset Workspace###AssetWorkspace", &open, WorkspaceFlags))
	{
		m_Open = open;
		m_ShortcutContextActive = false;
		ImGui::End();
		return;
	}

	m_ShortcutContextActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	if (m_ShortcutContextActive)
		m_FocusRequested = false;

	if (m_Fullscreen && TitlebarDragStarted())
		RestoreWorkspaceRect(true);
	else if (!m_Fullscreen && !ImGui::IsWindowDocked())
		CaptureWorkspaceRect();

	DrawWorkspaceHeader();
	ImGui::Separator();
	DrawWorkspaceTabs();

	m_Open = open;
	ImGui::End();

	if (!m_Open)
		CloseAll();
}

void AssetEditorPanel::HandleWorkspaceTabShortcut()
{
	if (m_Documents.size() < 2)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	if ((io.KeyAlt || io.KeyCtrl) && ImGui::IsKeyPressed(ImGuiKey_Tab, false))
		FocusNextEditor();
}

void AssetEditorPanel::FocusNextEditor()
{
	if (m_Documents.empty())
		return;

	size_t index = 0;
	for (size_t i = 0; i < m_Documents.size(); ++i)
	{
		if (m_Documents[i].m_Handle == m_ActiveDocument)
		{
			index = (i + 1) % m_Documents.size();
			break;
		}
	}

	m_ActiveDocument = m_Documents[index].m_Handle;
	for (AssetEditorDocument& document : m_Documents)
		document.m_FocusRequested = false;
	m_Documents[index].m_FocusRequested = true;
	m_Minimized = false;
	m_FocusRequested = true;
}

bool AssetEditorPanel::IsShortcutContextActive() const
{
	return m_Open && !m_Minimized && m_ShortcutContextActive;
}

AssetEditorPanel::AssetEditorDocument* AssetEditorPanel::GetActiveDocument()
{
	auto it = std::ranges::find_if(m_Documents, [this](const AssetEditorDocument& document)
	{
		return document.m_Open && document.m_Handle == m_ActiveDocument;
	});
	if (it != m_Documents.end())
		return &*it;

	return m_Documents.empty() ? nullptr : &m_Documents.front();
}

const AssetEditorPanel::AssetEditorDocument* AssetEditorPanel::GetActiveDocument() const
{
	auto it = std::ranges::find_if(m_Documents, [this](const AssetEditorDocument& document)
	{
		return document.m_Open && document.m_Handle == m_ActiveDocument;
	});
	if (it != m_Documents.end())
		return &*it;

	return m_Documents.empty() ? nullptr : &m_Documents.front();
}

bool AssetEditorPanel::CloseActiveEditor()
{
	AssetEditorDocument* activeDocument = GetActiveDocument();
	if (!activeDocument)
		return false;

	const AssetHandle activeHandle = activeDocument->m_Handle;
	std::erase_if(m_Documents, [activeHandle](const AssetEditorDocument& document)
	{
		return document.m_Handle == activeHandle;
	});

	if (m_Documents.empty())
	{
		CloseAll();
		return true;
	}

	m_ActiveDocument = m_Documents.front().m_Handle;
	m_Documents.front().m_FocusRequested = true;
	m_FocusRequested = true;
	m_OpenDirty = true;
	return true;
}

bool AssetEditorPanel::MinimizeWorkspace()
{
	if (!m_Open)
		return false;
	m_Minimized = true;
	m_Fullscreen = false;
	m_OpenDirty = true;
	MarkLayoutDirty();
	return true;
}

bool AssetEditorPanel::ToggleFullscreenWorkspace()
{
	if (!m_Open)
		return false;
	if (m_Fullscreen)
		RestoreWorkspaceRect();
	else
		RequestFullscreen();
	return true;
}

bool AssetEditorPanel::SetActiveTextureTool(TextureEditorTool tool)
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	document->m_TextureState.m_Tool = tool;
	return true;
}

bool AssetEditorPanel::SaveActiveTexture()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(document->m_Handle);
	if (!texture || !texture->IsLoaded() || !EnsureTextureEditorState(document->m_TextureState, document->m_Handle, texture))
		return false;

	ApplyTextureEditorState(document->m_TextureState, texture);
	return SaveTextureEditorState(document->m_TextureState, metadata);
}

bool AssetEditorPanel::ApplyActiveTexturePreview()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(document->m_Handle);
	if (!texture || !texture->IsLoaded() || !EnsureTextureEditorState(document->m_TextureState, document->m_Handle, texture))
		return false;

	ApplyTextureEditorState(document->m_TextureState, texture);
	document->m_TextureState.m_Status = "Preview applied.";
	return true;
}

bool AssetEditorPanel::ReloadActiveTexture()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(document->m_Handle);
	if (!texture || !texture->IsLoaded())
		return false;

	ReloadTextureEditorState(document->m_TextureState, document->m_Handle, texture);
	return true;
}

bool AssetEditorPanel::UndoActiveTextureEdit()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(document->m_Handle);
	return texture && texture->IsLoaded() && UndoTextureEdit(document->m_TextureState, texture);
}

bool AssetEditorPanel::RedoActiveTextureEdit()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(document->m_Handle);
	return texture && texture->IsLoaded() && RedoTextureEdit(document->m_TextureState, texture);
}

bool AssetEditorPanel::ResetActiveTextureView()
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	TextureEditorState& state = document->m_TextureState;
	const float longEdge = static_cast<float>(std::max(state.m_Width, state.m_Height));
	state.m_Zoom = std::clamp(512.0f / std::max(1.0f, longEdge), 1.0f, 24.0f);
	state.m_Pan = { 16.0f, 16.0f };
	return true;
}

bool AssetEditorPanel::ZoomActiveTexture(float multiplier)
{
	AssetEditorDocument* document = GetActiveDocument();
	if (!document || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager())
		return false;

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document->m_Handle);
	if (!metadata || metadata.m_Type != AssetType::Texture2D)
		return false;

	TextureEditorState& state = document->m_TextureState;
	state.m_Zoom = std::clamp(state.m_Zoom * multiplier, 1.0f, 96.0f);
	return true;
}

void AssetEditorPanel::DrawMinimizedStrip()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float stripHeight = 42.0f;
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + viewport->WorkSize.y - stripHeight - 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(std::min(viewport->WorkSize.x - 20.0f, 360.0f), stripHeight), ImGuiCond_Always);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	if (!ImGui::Begin("##MinimizedAssetWorkspace", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	std::string label = "Asset Workspace";
	if (m_ActiveDocument != 0 && Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(m_ActiveDocument))
	{
		if (const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(m_ActiveDocument); metadata)
			label += std::string("  ") + metadata.m_Filepath.filename().string();
	}
	label += "###RestoreAssetWorkspace";

	if (ImGui::Button(label.c_str(), ImVec2(330.0f, 24.0f)))
	{
		m_Minimized = false;
		m_FocusRequested = true;
		m_OpenDirty = true;
		MarkLayoutDirty();
	}

	ImGui::End();
}

void AssetEditorPanel::DrawWorkspaceHeader()
{
	ImGui::TextUnformatted("Asset Workspace");
	ImGui::SameLine();
	ImGui::TextDisabled("%zu open", m_Documents.size());

	const float controlsWidth = 28.0f * 2.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	if (ImGui::GetContentRegionAvail().x > controlsWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - controlsWidth);
	else
		ImGui::SameLine();

	if (DrawWindowControl("##AssetWorkspaceMinimize", WindowControlType::Minimize))
	{
		m_Minimized = true;
		m_Fullscreen = false;
		m_OpenDirty = true;
		MarkLayoutDirty();
	}
	ImGui::SameLine();
	if (DrawWindowControl("##AssetWorkspaceMaximize", m_Fullscreen ? WindowControlType::Restore : WindowControlType::Maximize))
	{
		if (m_Fullscreen)
			RestoreWorkspaceRect();
		else
			RequestFullscreen();
	}
}

void AssetEditorPanel::DrawWorkspaceTabs()
{
	if (m_ActiveDocument == 0 && !m_Documents.empty())
	{
		m_ActiveDocument = m_Documents.front().m_Handle;
		m_Documents.front().m_FocusRequested = true;
	}

	if (ImGui::BeginTabBar("##AssetWorkspaceTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (AssetEditorDocument& document : m_Documents)
		{
			if (!Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(document.m_Handle))
			{
				document.m_Open = false;
				continue;
			}

			const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document.m_Handle);
			if (!metadata)
			{
				document.m_Open = false;
				continue;
			}

			bool open = document.m_Open;
			const bool focusRequested = document.m_FocusRequested;
			ImGuiTabItemFlags flags = focusRequested ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			const std::string label = MakeTabLabel(document.m_Handle, metadata);
			if (ImGui::BeginTabItem(label.c_str(), &open, flags))
			{
				m_ActiveDocument = document.m_Handle;
				DrawDocumentContent(document);
				ImGui::EndTabItem();
			}
			if (focusRequested)
				document.m_FocusRequested = false;
			document.m_Open = open;
		}
		ImGui::EndTabBar();
	}

	for (size_t i = 0; i < m_Documents.size();)
	{
		if (!m_Documents[i].m_Open)
		{
			if (m_Documents[i].m_Handle == m_ActiveDocument)
				m_ActiveDocument = 0;
			if (m_Documents[i].m_Handle == m_EmbeddedAnimationHandle)
				m_EmbeddedAnimationHandle = 0;
			m_Documents.erase(m_Documents.begin() + static_cast<std::ptrdiff_t>(i));
			m_OpenDirty = true;
			continue;
		}
		++i;
	}

	if (m_ActiveDocument == 0 && !m_Documents.empty())
		m_ActiveDocument = m_Documents.front().m_Handle;
	if (m_Documents.empty())
	{
		m_Open = false;
		m_OpenDirty = true;
	}
}

void AssetEditorPanel::DrawDocumentContent(AssetEditorDocument& document)
{
	if (!Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(document.m_Handle))
	{
		document.m_Open = false;
		return;
	}

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document.m_Handle);
	if (!metadata)
	{
		document.m_Open = false;
		return;
	}

	DrawDocumentToolbar(document.m_Handle, metadata);
	ImGui::Separator();

	switch (metadata.m_Type) // NOLINT(clang-diagnostic-switch-enum)
	{
	case AssetType::Texture2D:
		DrawTextureInspector(document, metadata, false);
		break;
	case AssetType::Audio:
		DrawAudioInspector(document.m_Handle, false);
		break;
	case AssetType::Font:
		DrawFontInspector(document, metadata, false);
		break;
	case AssetType::Scene:
		DrawSceneInspector(document.m_Handle, false);
		break;
	case AssetType::Animation:
	case AssetType::AnimationController:
		DrawAnimationInspector(document.m_Handle, metadata, false);
		break;
	case AssetType::Entity:
		DrawEntityInspector(document.m_Handle, metadata, false);
		break;
	default:
		DrawMetadata(document.m_Handle, metadata);
		break;
	}
}

void AssetEditorPanel::DrawDocumentToolbar(AssetHandle handle, const AssetMetadata& metadata) const
{
	ImGui::TextUnformatted(AssetTypeName(metadata.m_Type));
	ImGui::SameLine();
	ImGui::TextDisabled("%s", metadata.m_Filepath.filename().string().c_str());

	constexpr float folderWidth = 58.0f;
	if (ImGui::GetContentRegionAvail().x > folderWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - folderWidth);
	else
		ImGui::NewLine();

	ImGui::PushID(static_cast<int>(handle));
	if (ImGui::SmallButton("Folder"))
		Utils::OpenExternalPath((Project::GetActiveAssetDirectory() / metadata.m_Filepath).parent_path());
	ImGui::PopID();
}

void AssetEditorPanel::CaptureWorkspaceRect()
{
	if (m_Fullscreen || ImGui::IsWindowDocked())
		return;

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	if (size.x <= 0.0f || size.y <= 0.0f)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (size.x >= viewport->WorkSize.x - 24.0f && size.y >= viewport->WorkSize.y - 24.0f)
		return;

	const glm::vec2 newPosition{ pos.x, pos.y };
	const glm::vec2 newSize{ size.x, size.y };
	if (!m_HasRestoreRect ||
		std::abs(m_RestorePosition.x - newPosition.x) > 0.5f ||
		std::abs(m_RestorePosition.y - newPosition.y) > 0.5f ||
		std::abs(m_RestoreSize.x - newSize.x) > 0.5f ||
		std::abs(m_RestoreSize.y - newSize.y) > 0.5f)
	{
		m_RestorePosition = newPosition;
		m_RestoreSize = newSize;
		m_HasRestoreRect = true;
		MarkLayoutDirty();
	}
}

void AssetEditorPanel::RequestFullscreen()
{
	CaptureWorkspaceRect();
	m_Minimized = false;
	m_Fullscreen = true;
	m_FullscreenRequested = true;
	m_FocusRequested = true;
	MarkLayoutDirty();
}

void AssetEditorPanel::RestoreWorkspaceRect(bool anchorToMouse)
{
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	const ImVec2 size(
		m_HasRestoreRect ? m_RestoreSize.x : 1040.0f,
		m_HasRestoreRect ? m_RestoreSize.y : 640.0f);
	ImVec2 pos(
		m_HasRestoreRect ? m_RestorePosition.x : ImGui::GetMainViewport()->WorkPos.x + 80.0f,
		m_HasRestoreRect ? m_RestorePosition.y : ImGui::GetMainViewport()->WorkPos.y + 80.0f);
	if (anchorToMouse)
	{
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const ImVec2 currentPos = ImGui::GetWindowPos();
		const ImVec2 currentSize = ImGui::GetWindowSize();
		const float mouseRatioX = currentSize.x > 1.0f ? std::clamp((mouse.x - currentPos.x) / currentSize.x, 0.08f, 0.92f) : 0.5f;
		pos.x = mouse.x - size.x * mouseRatioX;
		pos.y = mouse.y - ImGui::GetFrameHeight() * 0.5f;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		pos.x = std::clamp(pos.x, viewport->WorkPos.x, viewport->WorkPos.x + std::max(0.0f, viewport->WorkSize.x - size.x));
		pos.y = std::clamp(pos.y, viewport->WorkPos.y, viewport->WorkPos.y + std::max(0.0f, viewport->WorkSize.y - size.y));
	}
	ImGui::SetWindowPos(pos, ImGuiCond_Always);
	ImGui::SetWindowSize(size, ImGuiCond_Always);
	MarkLayoutDirty();
}

void AssetEditorPanel::MarkLayoutDirty()
{
	m_LayoutDirty = true;
}

void AssetEditorPanel::DrawMetadata(AssetHandle handle, const AssetMetadata& metadata) const
{
	const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;

	if (ImGui::BeginTable("##AssetMetadata", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
	{
		auto row = [](const char* key, const std::string& value)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", key);
				ImGui::TableNextColumn();
				ImGui::TextWrapped("%s", value.c_str());
			};

		row("Path", metadata.m_Filepath.generic_string());
		row("Type", AssetTypeName(metadata.m_Type));
		row("Size", FormatFileSize(absolutePath));
		row("Handle", std::to_string(static_cast<uint64_t>(handle)));
		ImGui::EndTable();
	}
}

bool AssetEditorPanel::EnsureTextureEditorState(TextureEditorState& state, AssetHandle handle, const Ref<Texture2D>& texture)
{
	if (!texture || !texture->IsLoaded())
		return false;

	const TextureSpecification& specification = texture->GetSpecification();
	const uint32_t channels = BytesPerPixel(specification.m_Format);
	if (channels == 0)
		return false;

	const uint64_t expectedSize = static_cast<uint64_t>(specification.m_Width) * specification.m_Height * channels;
	if (state.m_LoadedHandle == handle && state.m_Width == specification.m_Width && state.m_Height == specification.m_Height &&
		state.m_Channels == channels && state.m_Format == specification.m_Format && state.m_Pixels.size() == expectedSize)
		return true;

	ReloadTextureEditorState(state, handle, texture);
	return !state.m_Pixels.empty();
}

void AssetEditorPanel::ReloadTextureEditorState(TextureEditorState& state, AssetHandle handle, const Ref<Texture2D>& texture)
{
	state = TextureEditorState{};
	state.m_LoadedHandle = handle;

	if (!texture || !texture->IsLoaded())
	{
		state.m_Status = "Texture is not loaded.";
		return;
	}

	const TextureSpecification& specification = texture->GetSpecification();
	const uint32_t channels = BytesPerPixel(specification.m_Format);
	if (channels == 0)
	{
		state.m_Status = "Texture format is not editable.";
		return;
	}

	RawBuffer data = texture->GetData();
	if (!data)
	{
		state.m_Status = "Could not read texture pixels.";
		return;
	}

	state.m_Width = specification.m_Width;
	state.m_Height = specification.m_Height;
	state.m_Format = specification.m_Format;
	state.m_Channels = channels;
	state.m_Pixels.assign(data.m_Data, data.m_Data + data.m_Size);
	data.Release();

	const float longEdge = static_cast<float>(std::max(state.m_Width, state.m_Height));
	state.m_Zoom = std::clamp(512.0f / std::max(1.0f, longEdge), 1.0f, 24.0f);
	if (state.m_Zoom < 4.0f && longEdge <= 256.0f)
		state.m_Zoom = 4.0f;
	state.m_Pan = { 16.0f, 16.0f };
	state.m_Status = "Ready.";
}

void AssetEditorPanel::ApplyTextureEditorState(TextureEditorState& state, const Ref<Texture2D>& texture)
{
	if (!texture || !texture->IsLoaded() || state.m_Pixels.empty())
		return;

	RawBuffer data(state.m_Pixels.data(), static_cast<uint64_t>(state.m_Pixels.size()));
	texture->SetData(data);
	state.m_Status = state.m_Dirty ? "Preview updated. Save writes the PNG file." : "Preview updated.";
}

bool AssetEditorPanel::SaveTextureEditorState(TextureEditorState& state, const AssetMetadata& metadata)
{
	if (!Project::GetActive())
		return false;

	const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;
	if (LowercaseExtension(absolutePath) != ".png")
	{
		state.m_Status = "Direct save currently supports PNG textures. Live preview still uses your edits.";
		return false;
	}

	std::string error;
	if (!WritePngFile(absolutePath, state.m_Pixels, state.m_Width, state.m_Height, state.m_Channels, error))
	{
		state.m_Status = error;
		return false;
	}

	state.m_Dirty = false;
	state.m_Status = "Saved PNG.";
	return true;
}

bool AssetEditorPanel::SaveAssetMetadata(AssetHandle handle, const AssetMetadata& metadata)
{
	if (!Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return false;

	AssetMetadata normalizedMetadata = metadata;
	if (normalizedMetadata.m_Type == AssetType::Texture2D)
	{
		uint32_t textureWidth = 0;
		uint32_t textureHeight = 0;
		if (Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle); texture && texture->IsLoaded())
		{
			textureWidth = texture->GetWidth();
			textureHeight = texture->GetHeight();
		}
		Utils::NormalizeTextureSprites(normalizedMetadata.m_TextureSettings, textureWidth, textureHeight, normalizedMetadata.m_Filepath.stem().string());
	}

	const bool saved = Project::GetActive()->GetEditorAssetManager()->UpdateAssetMetadata(handle, normalizedMetadata);
	if (saved && m_RefreshAssetTreeCallback)
		m_RefreshAssetTreeCallback();
	return saved;
}

void AssetEditorPanel::HandleTextureEditorShortcuts(TextureEditorState& state, const Ref<Texture2D>& texture)
{
	ImGuiContext* context = ImGui::GetCurrentContext();
	if (!context)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput || context->ActiveId != 0 || io.KeyAlt)
		return;

	if (io.KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			UndoTextureEdit(state, texture);
			return;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
		{
			RedoTextureEdit(state, texture);
			return;
		}
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_B, false))
		state.m_Tool = TextureEditorTool::Brush;
	else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
		state.m_Tool = TextureEditorTool::Eraser;
	else if (ImGui::IsKeyPressed(ImGuiKey_P, false) || ImGui::IsKeyPressed(ImGuiKey_I, false))
		state.m_Tool = TextureEditorTool::Picker;
	else if (ImGui::IsKeyPressed(ImGuiKey_F, false))
		state.m_Tool = TextureEditorTool::Fill;
	else if (ImGui::IsKeyPressed(ImGuiKey_S, false))
		state.m_Tool = TextureEditorTool::Slice;
}

void AssetEditorPanel::PushTextureUndo(TextureEditorState& state)
{
	if (state.m_Pixels.empty())
		return;

	if (!state.m_UndoPixels.empty() && state.m_UndoPixels.back() == state.m_Pixels)
		return;

	static constexpr size_t MaxUndoSnapshots = 32;
	state.m_UndoPixels.push_back(state.m_Pixels);
	if (state.m_UndoPixels.size() > MaxUndoSnapshots)
		state.m_UndoPixels.erase(state.m_UndoPixels.begin());
	state.m_RedoPixels.clear();
}

bool AssetEditorPanel::UndoTextureEdit(TextureEditorState& state, const Ref<Texture2D>& texture)
{
	if (state.m_UndoPixels.empty())
	{
		state.m_Status = "Nothing to undo.";
		return false;
	}

	state.m_RedoPixels.push_back(state.m_Pixels);
	state.m_Pixels = std::move(state.m_UndoPixels.back());
	state.m_UndoPixels.pop_back();
	state.m_Dirty = true;
	ApplyTextureEditorState(state, texture);
	state.m_Status = "Undo texture edit.";
	return true;
}

bool AssetEditorPanel::RedoTextureEdit(TextureEditorState& state, const Ref<Texture2D>& texture)
{
	if (state.m_RedoPixels.empty())
	{
		state.m_Status = "Nothing to redo.";
		return false;
	}

	state.m_UndoPixels.push_back(state.m_Pixels);
	state.m_Pixels = std::move(state.m_RedoPixels.back());
	state.m_RedoPixels.pop_back();
	state.m_Dirty = true;
	ApplyTextureEditorState(state, texture);
	state.m_Status = "Redo texture edit.";
	return true;
}

std::array<uint8_t, 4> AssetEditorPanel::ReadTexturePixel(const TextureEditorState& state, int x, int y) const
{
	if (x < 0 || y < 0 || std::cmp_greater_equal(x, state.m_Width) || std::cmp_greater_equal(y, state.m_Height) || state.m_Pixels.empty())
		return { 0, 0, 0, 0 };

	const uint32_t storageY = state.m_Height - 1 - static_cast<uint32_t>(y);
	const size_t index = (static_cast<size_t>(storageY) * state.m_Width + static_cast<uint32_t>(x)) * state.m_Channels;
	return {
		state.m_Pixels[index + 0],
		state.m_Pixels[index + 1],
		state.m_Pixels[index + 2],
		state.m_Channels == 4 ? state.m_Pixels[index + 3] : static_cast<uint8_t>(255)
	};
}

bool AssetEditorPanel::WriteTexturePixel(TextureEditorState& state, int x, int y, const std::array<uint8_t, 4>& color)
{
	if (x < 0 || y < 0 || std::cmp_greater_equal(x, state.m_Width) || std::cmp_greater_equal(y, state.m_Height) || state.m_Pixels.empty())
		return false;

	const uint32_t storageY = state.m_Height - 1 - static_cast<uint32_t>(y);
	const size_t index = (static_cast<size_t>(storageY) * state.m_Width + static_cast<uint32_t>(x)) * state.m_Channels;
	bool changed = false;
	for (uint32_t channel = 0; channel < state.m_Channels; ++channel)
	{
		const uint8_t value = color[channel];
		if (state.m_Pixels[index + channel] != value)
		{
			state.m_Pixels[index + channel] = value;
			changed = true;
		}
	}
	return changed;
}

bool AssetEditorPanel::PaintTextureBrush(TextureEditorState& state, int x, int y, bool erase)
{
	std::array<uint8_t, 4> color = erase ? std::array<uint8_t, 4>{ 0, 0, 0, 0 } : BrushColorBytes(state.m_BrushColor);
	const int brushSize = std::max(1, state.m_BrushSize);
	const int minX = x - brushSize / 2;
	const int minY = y - brushSize / 2;
	const float radius = static_cast<float>(brushSize) * 0.5f;
	const float radiusSq = radius * radius;
	bool changed = false;

	for (int yy = minY; yy < minY + brushSize; ++yy)
	{
		for (int xx = minX; xx < minX + brushSize; ++xx)
		{
			const float dx = static_cast<float>(xx - minX) + 0.5f - radius;
			const float dy = static_cast<float>(yy - minY) + 0.5f - radius;
			if (brushSize > 2 && dx * dx + dy * dy > radiusSq)
				continue;
			changed |= WriteTexturePixel(state, xx, yy, color);
		}
	}

	if (changed)
		state.m_Dirty = true;
	return changed;
}

bool AssetEditorPanel::FillTextureRegion(TextureEditorState& state, int x, int y, const std::array<uint8_t, 4>& color)
{
	const std::array<uint8_t, 4> target = ReadTexturePixel(state, x, y);
	if (target == color)
		return false;

	std::vector<uint8_t> visited(static_cast<size_t>(state.m_Width) * state.m_Height, 0);
	std::deque<std::pair<int, int>> queue;
	queue.emplace_back(x, y);
	bool changed = false;

	while (!queue.empty())
	{
		const auto [cx, cy] = queue.front();
		queue.pop_front();
		if (cx < 0 || cy < 0 || std::cmp_greater_equal(cx, state.m_Width) || std::cmp_greater_equal(cy, state.m_Height))
			continue;

		const size_t visitIndex = static_cast<size_t>(cy) * state.m_Width + static_cast<uint32_t>(cx);
		if (visited[visitIndex])
			continue;
		visited[visitIndex] = 1;

		if (ReadTexturePixel(state, cx, cy) != target)
			continue;

		changed |= WriteTexturePixel(state, cx, cy, color);
		queue.emplace_back(cx + 1, cy);
		queue.emplace_back(cx - 1, cy);
		queue.emplace_back(cx, cy + 1);
		queue.emplace_back(cx, cy - 1);
	}

	if (changed)
		state.m_Dirty = true;
	return changed;
}

void AssetEditorPanel::DrawTextureInspector(AssetEditorDocument& document, const AssetMetadata& metadata, bool compact)
{
	AssetHandle handle = document.m_Handle;
	if (compact)
	{
		DrawMetadata(handle, metadata);
		return;
	}

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
	if (!texture || !texture->IsLoaded())
	{
		DrawMetadata(handle, metadata);
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Texture is not loaded.");
		return;
	}

	TextureEditorState& state = document.m_TextureState;
	if (!EnsureTextureEditorState(state, handle, texture))
	{
		DrawMetadata(handle, metadata);
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", state.m_Status.c_str());
		return;
	}

	AssetMetadata editedMetadata = metadata;
	TextureImportSettings& importSettings = editedMetadata.m_TextureSettings;
	if (std::cmp_greater_equal(state.m_SelectedSpriteIndex, importSettings.m_Sprites.size()))
		state.m_SelectedSpriteIndex = importSettings.m_Sprites.empty() ? -1 : static_cast<int>(importSettings.m_Sprites.size()) - 1;
	const float toolsWidth = std::min(360.0f, std::max(280.0f, ImGui::GetContentRegionAvail().x * 0.28f));
	ImGui::BeginChild("##TextureEditorTools", ImVec2(toolsWidth, 0.0f), true);
	ImGui::TextUnformatted("Texture Editor");
	ImGui::TextDisabled("%u x %u  %s", state.m_Width, state.m_Height, ImageFormatName(state.m_Format));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(TextureEditorToolColor(state.m_Tool)), "%s", TextureEditorToolName(state.m_Tool));
	ImGui::SameLine();
	ImGui::TextDisabled("- %s", TextureEditorToolHint(state.m_Tool));
	ImGui::Spacing();

	auto toolButton = [&](TextureEditorTool tool, const char* label, const char* shortcut)
	{
		const bool selected = state.m_Tool == tool;
		if (selected)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
		if (ImGui::Button(label, ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f)))
			state.m_Tool = tool;
		if (selected)
			ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", shortcut);
	};

	toolButton(TextureEditorTool::Brush, "Brush", "Shortcut: B");
	ImGui::SameLine();
	toolButton(TextureEditorTool::Eraser, "Eraser", "Shortcut: E");
	toolButton(TextureEditorTool::Picker, "Picker", "Shortcut: P or I");
	ImGui::SameLine();
	toolButton(TextureEditorTool::Fill, "Fill", "Shortcut: F");
	toolButton(TextureEditorTool::Slice, "Slice", "Shortcut: S");

	ImGui::SeparatorText("Paint");
	ImGui::PushItemWidth(TextureInspectorItemWidth());
	ImGui::ColorEdit4("Color", state.m_BrushColor.data(), ImGuiColorEditFlags_AlphaBar);
	ImGui::SliderInt("Brush Size", &state.m_BrushSize, 1, 32);
	ImGui::PopItemWidth();
	ImGui::Checkbox("Live Apply", &state.m_LiveApply);
	ImGui::Checkbox("Grid", &state.m_ShowGrid);
	ImGui::PushItemWidth(TextureInspectorItemWidth());
	ImGui::SliderFloat("Zoom", &state.m_Zoom, 1.0f, 64.0f, "%.1fx", ImGuiSliderFlags_Logarithmic);
	ImGui::PopItemWidth();

	if (ImGui::Button("Reset View", ImVec2(-1.0f, 0.0f)))
	{
		const float longEdge = static_cast<float>(std::max(state.m_Width, state.m_Height));
		state.m_Zoom = std::clamp(512.0f / std::max(1.0f, longEdge), 1.0f, 24.0f);
		state.m_Pan = { 16.0f, 16.0f };
	}

	ImGui::SeparatorText("Import Settings");
	bool importSettingsDirty = false;
	ImGui::PushItemWidth(TextureInspectorItemWidth());
	if (ImGui::BeginCombo("Filter", TextureFilterModeName(importSettings.m_FilterMode)))
	{
		for (TextureFilterMode mode : { TextureFilterMode::Nearest, TextureFilterMode::Linear })
		{
			const bool selected = importSettings.m_FilterMode == mode;
			if (ImGui::Selectable(TextureFilterModeName(mode), selected))
			{
				importSettings.m_FilterMode = mode;
				importSettingsDirty = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Wrap", TextureWrapModeName(importSettings.m_WrapMode)))
	{
		for (TextureWrapMode mode : { TextureWrapMode::Repeat, TextureWrapMode::ClampToEdge })
		{
			const bool selected = importSettings.m_WrapMode == mode;
			if (ImGui::Selectable(TextureWrapModeName(mode), selected))
			{
				importSettings.m_WrapMode = mode;
				importSettingsDirty = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	importSettingsDirty |= ImGui::Checkbox("Generate Mips", &importSettings.m_GenerateMips);
	importSettingsDirty |= ImGui::Checkbox("Alpha Transparency", &importSettings.m_AlphaTransparency);
	if (ImGui::BeginCombo("Sprite Mode", TextureSpriteModeName(importSettings.m_SpriteMode)))
	{
		for (TextureSpriteMode mode : { TextureSpriteMode::Single, TextureSpriteMode::Multiple })
		{
			const bool selected = importSettings.m_SpriteMode == mode;
			if (ImGui::Selectable(TextureSpriteModeName(mode), selected))
			{
				importSettings.m_SpriteMode = mode;
				importSettingsDirty = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	importSettingsDirty |= ImGui::DragFloat("Pixels Per Unit", &importSettings.m_PixelsPerUnit, 1.0f, 1.0f, 10000.0f, "%.0f");
	if (importSettings.m_PixelsPerUnit < 1.0f)
		importSettings.m_PixelsPerUnit = 1.0f;
	ImGui::PopItemWidth();
	if (importSettingsDirty && SaveAssetMetadata(handle, editedMetadata))
		state.m_Status = "Import settings saved. Reimport to apply sampler changes.";
	if (ImGui::Button("Reimport With Settings", ImVec2(-1.0f, 0.0f)))
	{
		if (Project::GetActive()->GetEditorAssetManager()->ReimportAsset(handle))
		{
			texture = AssetManager::GetAsset<Texture2D>(handle);
			ReloadTextureEditorState(state, handle, texture);
			state.m_Status = "Texture reimported with saved settings.";
		}
		else
		{
			state.m_Status = "Texture reimport failed.";
		}
	}

	if (importSettings.m_SpriteMode == TextureSpriteMode::Multiple)
	{
		ImGui::SeparatorText("Sprite Sheet");
		ImGui::PushItemWidth(TextureInspectorItemWidth());
		ImGui::InputInt("Cell W", &state.m_SliceCellWidth);
		ImGui::InputInt("Cell H", &state.m_SliceCellHeight);
		ImGui::InputInt("Padding", &state.m_SlicePadding);
		ImGui::InputInt("Spacing", &state.m_SliceSpacing);
		ImGui::PopItemWidth();
		state.m_SliceCellWidth = std::max(1, state.m_SliceCellWidth);
		state.m_SliceCellHeight = std::max(1, state.m_SliceCellHeight);
		state.m_SlicePadding = std::max(0, state.m_SlicePadding);
		state.m_SliceSpacing = std::max(0, state.m_SliceSpacing);
		if (ImGui::Button("Slice Grid", ImVec2(-1.0f, 0.0f)))
		{
			importSettings.m_Sprites.clear();
			int index = 0;
			for (int y = state.m_SlicePadding; y + state.m_SliceCellHeight <= static_cast<int>(state.m_Height); y += state.m_SliceCellHeight + state.m_SliceSpacing)
			{
				for (int x = state.m_SlicePadding; x + state.m_SliceCellWidth <= static_cast<int>(state.m_Width); x += state.m_SliceCellWidth + state.m_SliceSpacing)
				{
					TextureSpriteRect sprite;
					char nameBuffer[96]{};
					int result = std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03d", metadata.m_Filepath.stem().string().c_str(), index);

					if (result < 0 || std::cmp_greater_equal(result, sizeof(nameBuffer)))
						WHP_EDITOR_WARN("[Asset Editor] Buffer writing failed!");

					sprite.m_Name = nameBuffer;
					sprite.m_X = static_cast<uint32_t>(x);
					sprite.m_Y = static_cast<uint32_t>(y);
					sprite.m_Width = static_cast<uint32_t>(state.m_SliceCellWidth);
					sprite.m_Height = static_cast<uint32_t>(state.m_SliceCellHeight);
					importSettings.m_Sprites.push_back(std::move(sprite));
					++index;
				}
			}
			state.m_SelectedSpriteIndex = importSettings.m_Sprites.empty() ? -1 : 0;
			if (SaveAssetMetadata(handle, editedMetadata))
				state.m_Status = "Sprite grid generated.";
		}
		ImGui::SeparatorText("Smart Slice");
		ImGui::PushItemWidth(TextureInspectorItemWidth());
		ImGui::InputInt("Min Pixels", &state.m_AutoSliceMinPixels);
		ImGui::InputInt("Background Tolerance", &state.m_AutoSliceBackgroundTolerance);
		ImGui::InputInt("Fragment Merge Gap", &state.m_AutoSliceMergeGap);
		ImGui::InputInt("Auto Padding", &state.m_AutoSlicePadding);
		ImGui::InputInt("Extrude Pixels", &state.m_AutoSliceExtrudePixels);
		ImGui::PopItemWidth();
		state.m_AutoSliceMinPixels = std::max(1, state.m_AutoSliceMinPixels);
		state.m_AutoSliceBackgroundTolerance = std::clamp(state.m_AutoSliceBackgroundTolerance, 0, 255);
		state.m_AutoSliceMergeGap = std::max(0, state.m_AutoSliceMergeGap);
		state.m_AutoSlicePadding = std::max(0, state.m_AutoSlicePadding);
		state.m_AutoSliceExtrudePixels = std::clamp(state.m_AutoSliceExtrudePixels, 0, 16);
		ImGui::Checkbox("Separate Diagonal Touches", &state.m_AutoSliceSeparateDiagonalTouches);
		ImGui::Checkbox("Export Cropped PNGs", &state.m_AutoSliceExportPngs);
		if (ImGui::Button("Smart Auto Slice", ImVec2(-1.0f, 0.0f)))
		{
			TextureSlicer::PixelBuffer buffer;
			buffer.m_Width = state.m_Width;
			buffer.m_Height = state.m_Height;
			buffer.m_Channels = state.m_Channels;
			buffer.m_Format = state.m_Format;
			buffer.m_Pixels = state.m_Pixels;

			TextureSlicer::AutoSliceOptions options;
			options.m_MinPixels = static_cast<uint32_t>(state.m_AutoSliceMinPixels);
			options.m_BackgroundTolerance = state.m_AutoSliceBackgroundTolerance;
			options.m_MergeGap = static_cast<uint32_t>(state.m_AutoSliceMergeGap);
			options.m_Padding = static_cast<uint32_t>(state.m_AutoSlicePadding);
			options.m_ExtrudePixels = static_cast<uint32_t>(state.m_AutoSliceExtrudePixels);
			options.m_SeparateDiagonalTouches = state.m_AutoSliceSeparateDiagonalTouches;

			const std::string stem = metadata.m_Filepath.stem().empty() ? "sprite" : metadata.m_Filepath.stem().string();
			TextureSlicer::AutoSliceResult result = TextureSlicer::DetectSprites(buffer, stem, options);
			if (result.m_Sprites.empty())
			{
				state.m_Status = result.m_Error;
			}
			else
			{
				importSettings.m_SpriteMode = TextureSpriteMode::Multiple;
				importSettings.m_Sprites = result.m_Sprites;
				state.m_SelectedSpriteIndex = 0;
				bool saved = SaveAssetMetadata(handle, editedMetadata);
				size_t exportedCount = 0;
				bool exportFailed = false;
				if (saved && state.m_AutoSliceExportPngs && Project::GetActive())
				{
					const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
					const std::filesystem::path outputDirectory = assetDirectory / metadata.m_Filepath.parent_path() / (stem + "_slices");
					std::vector<std::filesystem::path> exportedPaths;
					std::string exportError;
					if (TextureSlicer::ExportSpritePngs(buffer, result, outputDirectory, stem, exportedPaths, exportError, options.m_ExtrudePixels))
					{
						exportedCount = exportedPaths.size();
						for (const std::filesystem::path& exportedPath : exportedPaths)
						{
							std::error_code relativeError;
							std::filesystem::path relativePath = std::filesystem::relative(exportedPath, assetDirectory, relativeError).lexically_normal();
							if (!relativeError)
								Project::GetActive()->GetEditorAssetManager()->ImportAsset(relativePath);
						}
					}
					else
					{
						state.m_Status = exportError;
						exportFailed = true;
					}
				}
				if (saved && !exportFailed)
				{
					state.m_Status = "Smart sliced " + std::to_string(result.m_Sprites.size()) + " sprite(s)";
					if (exportedCount > 0)
						state.m_Status += ", exported " + std::to_string(exportedCount) + " PNG(s)";
					state.m_Status += result.m_UsedAlpha ? " using alpha." : " using background model (tol " + std::to_string(result.m_EffectiveBackgroundTolerance) + ").";
				}
				else if (!saved)
				{
					state.m_Status = "Could not save smart slice metadata.";
				}
			}
		}
		if (ImGui::Button("Add Full Texture Sprite", ImVec2(-1.0f, 0.0f)))
		{
			TextureSpriteRect sprite;
			sprite.m_Name = metadata.m_Filepath.stem().string();
			sprite.m_X = 0;
			sprite.m_Y = 0;
			sprite.m_Width = state.m_Width;
			sprite.m_Height = state.m_Height;
			importSettings.m_Sprites.push_back(std::move(sprite));
			state.m_SelectedSpriteIndex = static_cast<int>(importSettings.m_Sprites.size()) - 1;
			if (SaveAssetMetadata(handle, editedMetadata))
				state.m_Status = "Sprite added.";
		}
		ImGui::BeginDisabled(importSettings.m_Sprites.empty());
		if (ImGui::Button("Normalize Sprite Metadata", ImVec2(-1.0f, 0.0f)))
		{
			const bool normalized = Utils::NormalizeTextureSprites(importSettings, state.m_Width, state.m_Height, metadata.m_Filepath.stem().string());
			if (!normalized)
				state.m_Status = "Sprite metadata already clean.";
			else if (SaveAssetMetadata(handle, editedMetadata))
				state.m_Status = "Sprite metadata normalized.";
			else
				state.m_Status = "Could not save normalized sprite metadata.";
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(importSettings.m_Sprites.empty());
		if (ImGui::Button("Clear Sprite Slices", ImVec2(-1.0f, 0.0f)))
		{
			const size_t removedCount = importSettings.m_Sprites.size();
			importSettings.m_Sprites.clear();
			state.m_SelectedSpriteIndex = -1;
			if (SaveAssetMetadata(handle, editedMetadata))
				state.m_Status = "Cleared " + std::to_string(removedCount) + " sprite slice(s).";
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("%zu sprite(s)", importSettings.m_Sprites.size());
		ImGui::BeginChild("##TextureSpriteList", ImVec2(0.0f, 118.0f), true);
		for (int i = 0; std::cmp_less(i, importSettings.m_Sprites.size()); ++i)
		{
			ImGui::PushID(i);
			const bool selected = state.m_SelectedSpriteIndex == i;
			if (ImGui::Selectable(importSettings.m_Sprites[i].m_Name.c_str(), selected))
				state.m_SelectedSpriteIndex = i;
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (state.m_SelectedSpriteIndex >= 0 && std::cmp_less(state.m_SelectedSpriteIndex, importSettings.m_Sprites.size()))
		{
			TextureSpriteRect& sprite = importSettings.m_Sprites[static_cast<size_t>(state.m_SelectedSpriteIndex)];
			bool spriteChanged = false;
			ImGui::PushItemWidth(TextureInspectorItemWidth());
			spriteChanged |= ImGui::InputText("Name", &sprite.m_Name);
			int rect[4] = {
				static_cast<int>(sprite.m_X),
				static_cast<int>(sprite.m_Y),
				static_cast<int>(sprite.m_Width),
				static_cast<int>(sprite.m_Height)
			};
			if (ImGui::DragInt4("Rect", rect, 1.0f, 0, 16384))
			{
				rect[0] = std::clamp(rect[0], 0, static_cast<int>(state.m_Width) - 1);
				rect[1] = std::clamp(rect[1], 0, static_cast<int>(state.m_Height) - 1);
				rect[2] = std::clamp(rect[2], 1, static_cast<int>(state.m_Width) - rect[0]);
				rect[3] = std::clamp(rect[3], 1, static_cast<int>(state.m_Height) - rect[1]);
				sprite.m_X = static_cast<uint32_t>(rect[0]);
				sprite.m_Y = static_cast<uint32_t>(rect[1]);
				sprite.m_Width = static_cast<uint32_t>(rect[2]);
				sprite.m_Height = static_cast<uint32_t>(rect[3]);
				spriteChanged = true;
			}
			ImGui::PopItemWidth();
			if (spriteChanged && SaveAssetMetadata(handle, editedMetadata))
				state.m_Status = "Sprite updated.";
			if (ImGui::Button("Duplicate Sprite", ImVec2(-1.0f, 0.0f)))
			{
				TextureSpriteRect duplicate = sprite;
				duplicate.m_Name += " Copy";
				importSettings.m_Sprites.insert(importSettings.m_Sprites.begin() + state.m_SelectedSpriteIndex + 1, std::move(duplicate));
				state.m_SelectedSpriteIndex += 1;
				Utils::NormalizeTextureSprites(importSettings, state.m_Width, state.m_Height, metadata.m_Filepath.stem().string());
				if (SaveAssetMetadata(handle, editedMetadata))
					state.m_Status = "Sprite duplicated.";
			}
			ImGui::BeginDisabled(state.m_SelectedSpriteIndex <= 0);
			if (ImGui::Button("Move Sprite Up", ImVec2(-1.0f, 0.0f)))
			{
				std::swap(importSettings.m_Sprites[static_cast<size_t>(state.m_SelectedSpriteIndex)], importSettings.m_Sprites[static_cast<size_t>(state.m_SelectedSpriteIndex - 1)]);
				state.m_SelectedSpriteIndex -= 1;
				if (SaveAssetMetadata(handle, editedMetadata))
					state.m_Status = "Sprite moved.";
			}
			ImGui::EndDisabled();
			ImGui::BeginDisabled(state.m_SelectedSpriteIndex + 1 >= static_cast<int>(importSettings.m_Sprites.size()));
			if (ImGui::Button("Move Sprite Down", ImVec2(-1.0f, 0.0f)))
			{
				std::swap(importSettings.m_Sprites[static_cast<size_t>(state.m_SelectedSpriteIndex)], importSettings.m_Sprites[static_cast<size_t>(state.m_SelectedSpriteIndex + 1)]);
				state.m_SelectedSpriteIndex += 1;
				if (SaveAssetMetadata(handle, editedMetadata))
					state.m_Status = "Sprite moved.";
			}
			ImGui::EndDisabled();
			if (ImGui::Button("Remove Sprite", ImVec2(-1.0f, 0.0f)))
			{
				importSettings.m_Sprites.erase(importSettings.m_Sprites.begin() + state.m_SelectedSpriteIndex);
				state.m_SelectedSpriteIndex = std::min(state.m_SelectedSpriteIndex, static_cast<int>(importSettings.m_Sprites.size()) - 1);
				if (SaveAssetMetadata(handle, editedMetadata))
					state.m_Status = "Sprite removed.";
			}
		}
	}

	ImGui::SeparatorText("File");
	if (ImGui::Button("Apply Preview", ImVec2(-1.0f, 0.0f)))
		ApplyTextureEditorState(state, texture);

	const bool canSavePng = LowercaseExtension(metadata.m_Filepath) == ".png";
	ImGui::BeginDisabled(!canSavePng || state.m_Pixels.empty());
	if (ImGui::Button(state.m_Dirty ? "Save PNG *" : "Save PNG", ImVec2(-1.0f, 0.0f)))
	{
		ApplyTextureEditorState(state, texture);
		SaveTextureEditorState(state, metadata);
	}
	ImGui::EndDisabled();
	if (!canSavePng && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Direct file save is limited to .png textures.");

	if (ImGui::Button("Reload From Asset", ImVec2(-1.0f, 0.0f)))
		ReloadTextureEditorState(state, handle, texture);
	if (ImGui::Button("Reimport Asset", ImVec2(-1.0f, 0.0f)))
	{
		if (Project::GetActive()->GetEditorAssetManager()->ReimportAsset(handle))
		{
			texture = AssetManager::GetAsset<Texture2D>(handle);
			ReloadTextureEditorState(state, handle, texture);
			state.m_Status = "Asset reimported.";
		}
		else
		{
			state.m_Status = "Asset reimport failed.";
		}
	}

	if (ImGui::CollapsingHeader("Asset Details"))
		DrawMetadata(handle, metadata);

	ImGui::Separator();
	if (state.m_Dirty)
		ImGui::TextColored(ImVec4(0.95f, 0.73f, 0.36f, 1.0f), "Unsaved changes");
	ImGui::TextWrapped("%s", state.m_Status.c_str());
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##TextureEditorCanvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	canvasSize.x = std::max(1.0f, canvasSize.x);
	canvasSize.y = std::max(1.0f, canvasSize.y);
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	ImGui::InvisibleButton("##TextureCanvasHitbox", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool canvasHovered = ImGui::IsItemHovered();
	const ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(9, 13, 16, 255));
	drawList->PushClipRect(canvasMin, canvasMax, true);
	{
		const ImVec2 badgeMin(canvasMin.x + 12.0f, canvasMin.y + 10.0f);
		const ImVec2 badgeMax(badgeMin.x + 172.0f, badgeMin.y + 28.0f);
		const ImU32 toolColor = TextureEditorToolColor(state.m_Tool);
		drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(12, 18, 24, 226), 5.0f);
		drawList->AddRectFilled(badgeMin, ImVec2(badgeMin.x + 4.0f, badgeMax.y), toolColor, 5.0f, ImDrawFlags_RoundCornersLeft);
		drawList->AddText(ImVec2(badgeMin.x + 12.0f, badgeMin.y + 6.0f), IM_COL32(226, 234, 240, 240), TextureEditorToolName(state.m_Tool));
		drawList->AddText(ImVec2(badgeMin.x + 72.0f, badgeMin.y + 6.0f), IM_COL32(142, 156, 166, 230), TextureEditorToolHint(state.m_Tool));
	}

	if (canvasHovered && io.MouseWheel != 0.0f)
	{
		const float oldZoom = state.m_Zoom;
		const ImVec2 oldImageMin(canvasMin.x + state.m_Pan.x, canvasMin.y + state.m_Pan.y);
		const float imageX = (io.MousePos.x - oldImageMin.x) / std::max(0.001f, oldZoom);
		const float imageY = (io.MousePos.y - oldImageMin.y) / std::max(0.001f, oldZoom);
		state.m_Zoom = std::clamp(state.m_Zoom * (1.0f + io.MouseWheel * 0.12f), 1.0f, 96.0f);
		state.m_Pan.x = io.MousePos.x - canvasMin.x - imageX * state.m_Zoom;
		state.m_Pan.y = io.MousePos.y - canvasMin.y - imageY * state.m_Zoom;
	}

	if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)))
	{
		state.m_Pan.x += io.MouseDelta.x;
		state.m_Pan.y += io.MouseDelta.y;
	}

	const ImVec2 imageMin(canvasMin.x + state.m_Pan.x, canvasMin.y + state.m_Pan.y);
	const ImVec2 imageMax(imageMin.x + static_cast<float>(state.m_Width) * state.m_Zoom, imageMin.y + static_cast<float>(state.m_Height) * state.m_Zoom);
	drawList->AddRectFilled(imageMin, imageMax, IM_COL32(28, 34, 38, 255));
	drawList->AddImage(UI::ToImGuiTextureId(texture->GetRendererId()), imageMin, imageMax, ImVec2(0, 1), ImVec2(1, 0));

	if (const bool drawGrid = state.m_ShowGrid && state.m_Zoom >= 6.0f && state.m_Width <= 1024 && state.m_Height <= 1024; drawGrid)
	{
		const ImU32 gridColor = IM_COL32(255, 255, 255, state.m_Zoom >= 12.0f ? 42 : 24);
		for (uint32_t x = 0; x <= state.m_Width; ++x)
		{
			const float screenX = imageMin.x + static_cast<float>(x) * state.m_Zoom;
			drawList->AddLine(ImVec2(screenX, imageMin.y), ImVec2(screenX, imageMax.y), gridColor);
		}
		for (uint32_t y = 0; y <= state.m_Height; ++y)
		{
			const float screenY = imageMin.y + static_cast<float>(y) * state.m_Zoom;
			drawList->AddLine(ImVec2(imageMin.x, screenY), ImVec2(imageMax.x, screenY), gridColor);
		}
	}
	if (importSettings.m_SpriteMode == TextureSpriteMode::Multiple)
	{
		for (int i = 0; std::cmp_less(i, importSettings.m_Sprites.size()); ++i)
		{
			const TextureSpriteRect& sprite = importSettings.m_Sprites[static_cast<size_t>(i)];
			if (sprite.m_Width == 0 || sprite.m_Height == 0)
				continue;

			const ImVec2 rectMin(imageMin.x + static_cast<float>(sprite.m_X) * state.m_Zoom, imageMin.y + static_cast<float>(sprite.m_Y) * state.m_Zoom);
			const ImVec2 rectMax(imageMin.x + static_cast<float>(sprite.m_X + sprite.m_Width) * state.m_Zoom, imageMin.y + static_cast<float>(sprite.m_Y + sprite.m_Height) * state.m_Zoom);
			const bool selected = state.m_SelectedSpriteIndex == i;
			drawList->AddRectFilled(rectMin, rectMax, selected ? IM_COL32(255, 190, 85, 26) : IM_COL32(88, 178, 214, 18));
			drawList->AddRect(rectMin, rectMax, selected ? IM_COL32(255, 198, 91, 230) : IM_COL32(92, 181, 218, 190), 0.0f, 0, selected ? 2.0f : 1.2f);
			if (state.m_Zoom >= 2.0f)
				drawList->AddText(ImVec2(rectMin.x + 4.0f, rectMin.y + 4.0f), selected ? IM_COL32(255, 238, 190, 240) : IM_COL32(180, 220, 236, 210), sprite.m_Name.c_str());
		}
	}
	drawList->AddRect(imageMin, imageMax, IM_COL32(126, 159, 184, 180), 0.0f, 0, 1.2f);

	const int pixelX = static_cast<int>((io.MousePos.x - imageMin.x) / std::max(0.001f, state.m_Zoom));
	const int pixelY = static_cast<int>((io.MousePos.y - imageMin.y) / std::max(0.001f, state.m_Zoom));
	const bool overPixel = canvasHovered && pixelX >= 0 && pixelY >= 0 && std::cmp_less(pixelX, state.m_Width) && std::cmp_less(pixelY, state.m_Height);
	bool changed = false;

	if (overPixel)
	{
		if (state.m_Tool == TextureEditorTool::Slice && importSettings.m_SpriteMode == TextureSpriteMode::Multiple)
		{
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				state.m_IsSlicing = true;
				state.m_SliceStart = { static_cast<float>(pixelX), static_cast<float>(pixelY) };
				state.m_SliceEnd = state.m_SliceStart;
			}
			if (state.m_IsSlicing && ImGui::IsMouseDown(ImGuiMouseButton_Left))
				state.m_SliceEnd = { static_cast<float>(pixelX), static_cast<float>(pixelY) };
			if (state.m_IsSlicing && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				const int minX = std::clamp(static_cast<int>(std::min(state.m_SliceStart.x, state.m_SliceEnd.x)), 0, static_cast<int>(state.m_Width) - 1);
				const int minY = std::clamp(static_cast<int>(std::min(state.m_SliceStart.y, state.m_SliceEnd.y)), 0, static_cast<int>(state.m_Height) - 1);
				const int maxX = std::clamp(static_cast<int>(std::max(state.m_SliceStart.x, state.m_SliceEnd.x)), 0, static_cast<int>(state.m_Width) - 1);
				const int maxY = std::clamp(static_cast<int>(std::max(state.m_SliceStart.y, state.m_SliceEnd.y)), 0, static_cast<int>(state.m_Height) - 1);
				if (maxX >= minX && maxY >= minY)
				{
					TextureSpriteRect sprite;
					char nameBuffer[96]{};
					int result = std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03zu", metadata.m_Filepath.stem().string().c_str(), importSettings.m_Sprites.size());

					if (result < 0 || std::cmp_greater_equal(result, sizeof(nameBuffer)))
						WHP_EDITOR_WARN("[Asset Editor] Buffer writing failed!");
					sprite.m_Name = nameBuffer;
					sprite.m_X = static_cast<uint32_t>(minX);
					sprite.m_Y = static_cast<uint32_t>(minY);
					sprite.m_Width = static_cast<uint32_t>(maxX - minX + 1);
					sprite.m_Height = static_cast<uint32_t>(maxY - minY + 1);
					importSettings.m_Sprites.push_back(std::move(sprite));
					state.m_SelectedSpriteIndex = static_cast<int>(importSettings.m_Sprites.size()) - 1;
					if (SaveAssetMetadata(handle, editedMetadata))
						state.m_Status = "Sprite slice added.";
				}
				state.m_IsSlicing = false;
			}
		}
		else
		{
			const int brushSize = std::max(1, state.m_BrushSize);
			const int minX = pixelX - brushSize / 2;
			const int minY = pixelY - brushSize / 2;
			const ImVec2 brushMin(imageMin.x + static_cast<float>(minX) * state.m_Zoom, imageMin.y + static_cast<float>(minY) * state.m_Zoom);
			const ImVec2 brushMax(imageMin.x + static_cast<float>(minX + brushSize) * state.m_Zoom, imageMin.y + static_cast<float>(minY + brushSize) * state.m_Zoom);
			drawList->AddRect(brushMin, brushMax, IM_COL32(255, 255, 255, 210), 0.0f, 0, 1.5f);

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (ImGui::IsMouseDown(ImGuiMouseButton_Left) && (state.m_Tool == TextureEditorTool::Brush || state.m_Tool == TextureEditorTool::Eraser)))
			{
				const bool mutatesPixels = state.m_Tool == TextureEditorTool::Brush || state.m_Tool == TextureEditorTool::Eraser || state.m_Tool == TextureEditorTool::Fill;
				const bool startsNewStroke = ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
					(!state.m_PaintStrokeActive && ImGui::IsMouseDown(ImGuiMouseButton_Left) && (state.m_Tool == TextureEditorTool::Brush || state.m_Tool == TextureEditorTool::Eraser));
				if (mutatesPixels && startsNewStroke)
				{
					PushTextureUndo(state);
					state.m_PaintStrokeActive = true;
				}
				switch (state.m_Tool)
				{
				case TextureEditorTool::Brush:
					changed = PaintTextureBrush(state, pixelX, pixelY, false);
					break;
				case TextureEditorTool::Eraser:
					changed = PaintTextureBrush(state, pixelX, pixelY, true);
					break;
				case TextureEditorTool::Picker:
				{
					const std::array<uint8_t, 4> color = ReadTexturePixel(state, pixelX, pixelY);
					state.m_BrushColor = {
						static_cast<float>(color[0]) / 255.0f,
						static_cast<float>(color[1]) / 255.0f,
						static_cast<float>(color[2]) / 255.0f,
						static_cast<float>(color[3]) / 255.0f
					};
					state.m_Status = "Picked color.";
					break;
				}
				case TextureEditorTool::Fill:
					changed = FillTextureRegion(state, pixelX, pixelY, BrushColorBytes(state.m_BrushColor));
					break;
				case TextureEditorTool::Slice:
					break;
				}
			}
		}
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		state.m_PaintStrokeActive = false;

	if (state.m_IsSlicing && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !overPixel)
		state.m_IsSlicing = false;

	if (state.m_IsSlicing)
	{
		const float minX = std::min(state.m_SliceStart.x, state.m_SliceEnd.x);
		const float minY = std::min(state.m_SliceStart.y, state.m_SliceEnd.y);
		const float maxX = std::max(state.m_SliceStart.x, state.m_SliceEnd.x) + 1.0f;
		const float maxY = std::max(state.m_SliceStart.y, state.m_SliceEnd.y) + 1.0f;
		const ImVec2 rectMin(imageMin.x + minX * state.m_Zoom, imageMin.y + minY * state.m_Zoom);
		const ImVec2 rectMax(imageMin.x + maxX * state.m_Zoom, imageMin.y + maxY * state.m_Zoom);
		drawList->AddRectFilled(rectMin, rectMax, IM_COL32(255, 194, 82, 36));
		drawList->AddRect(rectMin, rectMax, IM_COL32(255, 210, 104, 240), 0.0f, 0, 2.0f);
	}

	if (changed && state.m_LiveApply)
		ApplyTextureEditorState(state, texture);

	if (overPixel)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		const ImVec2 mouse = io.MousePos;
		const ImU32 cursorColor = TextureEditorToolColor(state.m_Tool);
		if (state.m_Tool == TextureEditorTool::Brush || state.m_Tool == TextureEditorTool::Eraser)
		{
			const float halfSize = std::max(4.0f, static_cast<float>(std::max(1, state.m_BrushSize)) * state.m_Zoom * 0.5f);
			drawList->AddRect(ImVec2(mouse.x - halfSize, mouse.y - halfSize), ImVec2(mouse.x + halfSize, mouse.y + halfSize), cursorColor, 2.0f, 0, 1.8f);
			if (state.m_Tool == TextureEditorTool::Eraser)
				drawList->AddLine(ImVec2(mouse.x - halfSize, mouse.y + halfSize), ImVec2(mouse.x + halfSize, mouse.y - halfSize), cursorColor, 1.6f);
		}
		else if (state.m_Tool == TextureEditorTool::Picker)
		{
			drawList->AddCircle(mouse, 8.0f, cursorColor, 16, 1.8f);
			drawList->AddLine(ImVec2(mouse.x - 12.0f, mouse.y), ImVec2(mouse.x + 12.0f, mouse.y), cursorColor, 1.4f);
			drawList->AddLine(ImVec2(mouse.x, mouse.y - 12.0f), ImVec2(mouse.x, mouse.y + 12.0f), cursorColor, 1.4f);
		}
		else if (state.m_Tool == TextureEditorTool::Fill)
		{
			drawList->AddTriangleFilled(ImVec2(mouse.x, mouse.y - 10.0f), ImVec2(mouse.x + 10.0f, mouse.y + 8.0f), ImVec2(mouse.x - 10.0f, mouse.y + 8.0f), cursorColor);
			drawList->AddCircleFilled(ImVec2(mouse.x + 11.0f, mouse.y + 11.0f), 3.0f, cursorColor);
		}
		else if (state.m_Tool == TextureEditorTool::Slice)
		{
			drawList->AddRect(ImVec2(mouse.x - 10.0f, mouse.y - 8.0f), ImVec2(mouse.x + 10.0f, mouse.y + 8.0f), cursorColor, 0.0f, 0, 1.8f);
			drawList->AddLine(ImVec2(mouse.x - 14.0f, mouse.y), ImVec2(mouse.x + 14.0f, mouse.y), cursorColor, 1.2f);
			drawList->AddLine(ImVec2(mouse.x, mouse.y - 12.0f), ImVec2(mouse.x, mouse.y + 12.0f), cursorColor, 1.2f);
		}
	}

	drawList->PopClipRect();
	if (overPixel)
	{
		const std::array<uint8_t, 4> color = ReadTexturePixel(state, pixelX, pixelY);
		ImGui::SetTooltip("X %d  Y %d\nRGBA %u %u %u %u", pixelX, pixelY, color[0], color[1], color[2], color[3]);
	}
	ImGui::EndChild();
}

void AssetEditorPanel::DrawAudioInspector(AssetHandle handle, bool compact) const
{
	const AssetMetadata& metadata = AssetManager::GetAssetMetadata(handle);
	if (compact)
	{
		DrawMetadata(handle, metadata);
		return;
	}

	Ref<AudioSource> audio = AssetManager::GetAsset<AudioSource>(handle);
	if (!audio || !audio->IsLoaded())
	{
		DrawMetadata(handle, metadata);
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Audio is not loaded.");
		return;
	}

	const float inspectorWidth = std::min(320.0f, std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.32f));
	ImGui::BeginChild("##AudioEditorControls", ImVec2(inspectorWidth, 0.0f), true);
	ImGui::TextUnformatted("Audio Editor");
	ImGui::TextDisabled("%s", FormatDuration(audio->GetLength()).c_str());
	ImGui::Spacing();

	if (ImGui::Button("Play", ImVec2(72.0f, 0.0f)))
		AudioEngine::Play(audio);
	ImGui::SameLine();
	if (ImGui::Button("Pause", ImVec2(72.0f, 0.0f)))
		AudioEngine::Pause(audio);
	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(72.0f, 0.0f)))
		AudioEngine::Stop(audio);
	if (ImGui::Button("Rewind", ImVec2(-1.0f, 0.0f)))
		AudioEngine::Rewind(audio);

	float position = audio->GetCurrentDuration();
	if (ImGui::SliderFloat("Position", &position, 0.0f, std::max(0.01f, audio->GetLength()), "%.2fs"))
		AudioEngine::Seek(audio, position);

	float gain = audio->GetGain();
	if (ImGui::SliderFloat("Gain", &gain, 0.0f, 2.0f, "%.2f"))
		audio->SetGain(gain);

	float pitch = audio->GetPitch();
	if (ImGui::SliderFloat("Pitch", &pitch, 0.25f, 3.0f, "%.2f"))
		audio->SetPitch(pitch);

	bool loop = audio->IsLoop();
	if (ImGui::Checkbox("Loop", &loop))
		audio->SetLoop(loop);

	bool spatial = audio->IsSpitial();
	if (ImGui::Checkbox("Spatial", &spatial))
		audio->SetSpitial(spatial);

	if (ImGui::CollapsingHeader("Asset Details"))
		DrawMetadata(handle, metadata);
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##AudioEditorReadout", ImVec2(0.0f, 0.0f), true);
	const AudioEngine::AudioState state = AudioEngine::GetState(audio);
	const char* stateText = "None";
	switch (state) // NOLINT(clang-diagnostic-switch-enum)
	{
	case AudioEngine::AudioState::Stopped: stateText = "Stopped"; break;
	case AudioEngine::AudioState::Playing: stateText = "Playing"; break;
	case AudioEngine::AudioState::Paused: stateText = "Paused"; break;
	default: break;
	}

	ImGui::SeparatorText("Clip");
	ImGui::Text("State: %s", stateText);
	ImGui::Text("Length: %s", FormatDuration(audio->GetLength()).c_str());
	ImGui::Text("Streaming: %s", audio->IsStreaming() ? "true" : "false");

	const float waveformHeight = std::min(160.0f, std::max(96.0f, ImGui::GetContentRegionAvail().y * 0.35f));
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const ImVec2 size(ImGui::GetContentRegionAvail().x, waveformHeight);
	ImGui::InvisibleButton("##AudioWaveformPlaceholder", size);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(10, 16, 20, 255), 4.0f);
	constexpr ImU32 lineColor = IM_COL32(88, 145, 178, 180);
	const float centerY = min.y + size.y * 0.5f;
	for (int i = 0; i < 96; ++i)
	{
		const float t = static_cast<float>(i) / 95.0f;
		const float x = min.x + t * size.x;
		const float height = (0.18f + 0.72f * std::abs(std::sin(t * 19.0f) * std::cos(t * 7.0f))) * size.y * 0.5f;
		drawList->AddLine(ImVec2(x, centerY - height), ImVec2(x, centerY + height), lineColor, 1.0f);
	}
	drawList->AddRect(min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(80, 104, 120, 180), 4.0f);
	ImGui::TextDisabled("Waveform visualization is generated as an editor guide.");
	ImGui::EndChild();
}

void AssetEditorPanel::DrawFontInspector(AssetEditorDocument& document, const AssetMetadata& metadata, bool compact) const
{
	AssetHandle handle = document.m_Handle;
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	Ref<Font> font = AssetManager::GetAsset<Font>(handle);
	if (!font)
	{
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Font is not loaded.");
		return;
	}

	Ref<Texture2D> atlas = font->GetAtlasTexture();
	if (!atlas || !atlas->IsLoaded())
	{
		ImGui::TextDisabled("Font atlas is not available.");
		return;
	}

	FontEditorState& state = document.m_FontState;
	ImGui::SeparatorText("Font Editor");
	ImGui::InputTextMultiline("Preview Text", &state.m_PreviewText, ImVec2(-1.0f, 88.0f));
	ImGui::SliderFloat("Preview Scale", &state.m_PreviewScale, 0.5f, 3.0f, "%.1fx");
	ImGui::Checkbox("Atlas Grid", &state.m_ShowAtlasGrid);

	const float previewHeight = std::max(90.0f, 110.0f * state.m_PreviewScale);
	ImGui::BeginChild("##FontPreviewSurface", ImVec2(0.0f, previewHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetWindowPos();
	const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
	drawList->AddRectFilled(min, max, IM_COL32(8, 13, 16, 255), 4.0f);
	const uint32_t previewWidth = static_cast<uint32_t>(std::max(64.0f, max.x - min.x));
	const uint32_t previewHeightPx = static_cast<uint32_t>(std::max(64.0f, max.y - min.y));
	if (!state.m_PreviewTexture || state.m_PreviewTextureFont != handle || state.m_PreviewTextureText != state.m_PreviewText ||
		!Math::EqualF(state.m_PreviewScale, state.m_PreviewTextureScale) || state.m_PreviewTextureWidth != previewWidth || state.m_PreviewTextureHeight != previewHeightPx)
	{
		state.m_PreviewTexture = BuildFontPreviewTexture(font, state.m_PreviewText, state.m_PreviewScale, previewWidth, previewHeightPx);
		state.m_PreviewTextureFont = handle;
		state.m_PreviewTextureText = state.m_PreviewText;
		state.m_PreviewTextureScale = state.m_PreviewScale;
		state.m_PreviewTextureWidth = previewWidth;
		state.m_PreviewTextureHeight = previewHeightPx;
	}
	if (state.m_PreviewTexture)
		drawList->AddImage(UI::ToImGuiTextureId(state.m_PreviewTexture->GetRendererId()), min, max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
	ImGui::EndChild();

	ImGui::SeparatorText("Atlas");
	ImGui::TextDisabled("%u x %u", atlas->GetWidth(), atlas->GetHeight());
	const ImVec2 previewSize = FitImageSize(static_cast<float>(atlas->GetWidth()), static_cast<float>(atlas->GetHeight()), ImGui::GetContentRegionAvail());
	const ImVec2 imageMin = ImGui::GetCursorScreenPos();
	UI::Image(UI::ToImGuiTextureId(atlas->GetRendererId()), previewSize, { 0, 1 }, { 1, 0 });
	if (state.m_ShowAtlasGrid)
	{
		ImDrawList* atlasDrawList = ImGui::GetWindowDrawList();
		const ImVec2 imageMax(imageMin.x + previewSize.x, imageMin.y + previewSize.y);
		constexpr ImU32 gridColor = IM_COL32(255, 255, 255, 28);
		for (int i = 1; i < 8; ++i)
		{
			const float x = imageMin.x + previewSize.x * static_cast<float>(i) / 8.0f;
			const float y = imageMin.y + previewSize.y * static_cast<float>(i) / 8.0f;
			atlasDrawList->AddLine(ImVec2(x, imageMin.y), ImVec2(x, imageMax.y), gridColor);
			atlasDrawList->AddLine(ImVec2(imageMin.x, y), ImVec2(imageMax.x, y), gridColor);
		}
	}
}

void AssetEditorPanel::DrawSceneInspector(AssetHandle handle, bool compact) const
{
	const AssetMetadata& metadata = AssetManager::GetAssetMetadata(handle);
	if (compact)
	{
		DrawMetadata(handle, metadata);
		return;
	}

	DrawMetadata(handle, metadata);
	ImGui::SeparatorText("Scene Editor");
	if (ImGui::Button("Open Scene", ImVec2(120.0f, 0.0f)) && m_OpenSceneCallback)
		m_OpenSceneCallback(handle);
	ImGui::SameLine();
	const bool isStartScene = Project::GetActive() && Project::GetActive()->GetConfig().m_StartScene == handle;
	ImGui::BeginDisabled(isStartScene);
	if (ImGui::Button(isStartScene ? "Start Scene" : "Set Start Scene", ImVec2(128.0f, 0.0f)) && m_SetStartSceneCallback)
		m_SetStartSceneCallback(handle);
	ImGui::EndDisabled();

	const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;
	if (ImGui::CollapsingHeader("Scene Source Preview"))
	{
		const std::string preview = ReadTextPreview(absolutePath);
		ImGui::BeginChild("##SceneSourcePreview", ImVec2(0.0f, 260.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
		if (preview.empty())
			ImGui::TextDisabled("Scene file could not be read.");
		else
			ImGui::TextUnformatted(preview.c_str());
		ImGui::EndChild();
	}
}

void AssetEditorPanel::DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact)
{
	if (compact)
	{
		DrawMetadata(handle, metadata);
		return;
	}

	if (m_OpenAnimationCallback && m_EmbeddedAnimationHandle != handle)
	{
		if (m_OpenAnimationCallback(handle))
			m_EmbeddedAnimationHandle = handle;
	}

	if (m_DrawAnimationEditorCallback && m_EmbeddedAnimationHandle == handle)
	{
		m_DrawAnimationEditorCallback();
		return;
	}

	DrawMetadata(handle, metadata);
	ImGui::SeparatorText("Animation");
	ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Animation editor could not load this asset.");
}

void AssetEditorPanel::DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	ImGui::SeparatorText("Entity Template Editor");
	const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;
	if (ImGui::Button("Open Source", ImVec2(128.0f, 0.0f)))
		Utils::OpenExternalPath(absolutePath);
	ImGui::SameLine();
	if (ImGui::Button("Show In Folder", ImVec2(128.0f, 0.0f)))
		Utils::OpenExternalPath(absolutePath.parent_path());

	ImGui::TextDisabled("Template source");
	const std::string preview = ReadTextPreview(absolutePath);
	ImGui::BeginChild("##EntityTemplateSource", ImVec2(0.0f, 320.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
	if (preview.empty())
		ImGui::TextDisabled("Entity template file could not be read.");
	else
		ImGui::TextUnformatted(preview.c_str());
	ImGui::EndChild();
}

std::string AssetEditorPanel::MakeTabLabel(AssetHandle handle, const AssetMetadata& metadata) const
{
	std::string label = metadata.m_Filepath.filename().string();
	if (label.empty())
		label = AssetTypeName(metadata.m_Type);
	label += "###AssetTab_";
	label += std::to_string(static_cast<uint64_t>(handle));
	return label;
}

_WHIP_END
