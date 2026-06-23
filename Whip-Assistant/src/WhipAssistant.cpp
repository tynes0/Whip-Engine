#include <Whip-Assistant/WhipAssistant.h>

#include <Whip-Assistant/AssistantToolRegistry.h>
#include <Whip-Assistant/Providers/GeminiProvider.h>
#include <Whip-Assistant/Providers/IAssistantProvider.h>
#include <Whip-Assistant/Providers/OpenAIProvider.h>

#include "Providers/AssistantProviderUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

_WHIP_START

namespace Assistant
{
	namespace
	{
		std::string LowerCopy(std::string value)
		{
			std::ranges::transform(value, value.begin(),
				[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
			return value;
		}

		std::string NormalizeName(std::string_view value)
		{
			std::string result;
			result.reserve(value.size());
			for (const unsigned char character : value)
			{
				if (std::isalnum(character))
					result += static_cast<char>(std::tolower(character));
			}
			return result;
		}

		bool Contains(std::string_view haystack, std::string_view needle)
		{
			return haystack.find(needle) != std::string_view::npos;
		}

		bool ContainsAny(std::string_view haystack, std::initializer_list<std::string_view> needles)
		{
			for (std::string_view needle : needles)
				if (Contains(haystack, needle))
					return true;
			return false;
		}

		std::string FormatVec3(const glm::vec3& value)
		{
			std::ostringstream stream;
			stream << std::fixed << std::setprecision(2) << value.x << ',' << value.y << ',' << value.z;
			return stream.str();
		}

		std::string TrimCopy(std::string_view value)
		{
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
				value.remove_prefix(1);
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
				value.remove_suffix(1);
			return std::string(value);
		}

		bool StartsWithNoCase(std::string_view value, std::string_view prefix)
		{
			if (value.size() < prefix.size())
				return false;

			for (size_t i = 0; i < prefix.size(); ++i)
				if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
					return false;
			return true;
		}

		std::string StripSingleEdgeNewline(std::string value)
		{
			if (value.starts_with("\r\n"))
				value.erase(0, 2);
			else if (value.starts_with('\n') || value.starts_with('\r'))
				value.erase(0, 1);

			if (value.ends_with("\r\n"))
				value.erase(value.size() - 2);
			else if (value.ends_with('\n') || value.ends_with('\r'))
				value.erase(value.size() - 1);
			return value;
		}

		std::optional<uint64_t> ParseUInt64(std::string_view value)
		{
			try
			{
				std::string text = TrimCopy(value);
				if (text.empty())
					return std::nullopt;
				size_t parsed = 0;
				const uint64_t result = std::stoull(text, &parsed, 10);
				return parsed == text.size() ? std::optional<uint64_t>(result) : std::nullopt;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		std::optional<int32_t> ParseInt32(std::string_view value)
		{
			try
			{
				std::string text = TrimCopy(value);
				if (text.empty())
					return std::nullopt;
				size_t parsed = 0;
				const int result = std::stoi(text, &parsed, 10);
				return parsed == text.size() ? std::optional<int32_t>(static_cast<int32_t>(result)) : std::nullopt;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		std::optional<float> ParseFloat(std::string_view value)
		{
			try
			{
				std::string text = TrimCopy(value);
				if (text.empty())
					return std::nullopt;
				size_t parsed = 0;
				const float result = std::stof(text, &parsed);
				return parsed == text.size() ? std::optional<float>(result) : std::nullopt;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		const char* AssetTypeName(AssetType type)
		{
			switch (type)
			{
			case AssetType::Scene: return "Scene";
			case AssetType::Texture2D: return "Texture2D";
			case AssetType::Audio: return "Audio";
			case AssetType::Font: return "Font";
			case AssetType::Animation: return "Animation";
			case AssetType::AnimationController: return "AnimationController";
			case AssetType::Entity: return "Entity";
			case AssetType::None:
			default: return "None";
			}
		}

		AssetType ParseAssetType(std::string value)
		{
			const std::string lower = LowerCopy(std::move(value));
			if (lower == "scene")
				return AssetType::Scene;
			if (lower == "texture" || lower == "texture2d" || lower == "sprite")
				return AssetType::Texture2D;
			if (lower == "audio" || lower == "sound")
				return AssetType::Audio;
			if (lower == "font")
				return AssetType::Font;
			if (lower == "animation")
				return AssetType::Animation;
			if (lower == "animationcontroller" || lower == "controller")
				return AssetType::AnimationController;
			if (lower == "entity" || lower == "entitytemplate" || lower == "prefab")
				return AssetType::Entity;
			return AssetType::None;
		}

		bool ParseVector3(std::string value, glm::vec3& result)
		{
			for (char& character : value)
			{
				if (character == ',' || character == ';' || character == '|' || character == '(' || character == ')' || character == '[' || character == ']')
					character = ' ';
			}

			std::stringstream stream(value);
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (!(stream >> x >> y >> z))
				return false;

			result = { x, y, z };
			return true;
		}

		bool ParseFlexibleVector3(std::string value, glm::vec3& result)
		{
			for (char& character : value)
			{
				if (character == ',' || character == ';' || character == '|' || character == '(' || character == ')' || character == '[' || character == ']')
					character = ' ';
			}

			std::stringstream stream(value);
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (!(stream >> x >> y))
				return false;
			if (!(stream >> z))
				z = 0.0f;

			result = { x, y, z };
			return true;
		}

		struct ToolBlockField
		{
			std::string m_Key;
			std::string m_Value;
		};

		size_t CountLeadingWhitespace(std::string_view value)
		{
			size_t count = 0;
			while (count < value.size() && (value[count] == ' ' || value[count] == '\t'))
				++count;
			return count;
		}

		bool IsPlacementInlineKey(const std::string& key)
		{
			return key == "name" ||
				key == "entity" ||
				key == "entityname" ||
				key == "sprite" ||
				key == "spritename" ||
				key == "spriteindex" ||
				key == "index" ||
				key == "subresourceindex" ||
				key == "assethandle" ||
				key == "handle" ||
				key == "assetid" ||
				key == "assetpath" ||
				key == "assetname" ||
				key == "asset" ||
				key == "path" ||
				key == "position" ||
				key == "translation" ||
				key == "pos" ||
				key == "scale" ||
				key == "size" ||
				key == "rotation" ||
				key == "rotationz" ||
				key == "angle";
		}

		bool IsPlacementBlockKey(const std::string& key)
		{
			return key == "placement" || key == "placements" || key == "levelplacements";
		}

		void PushStructuredPlacement(std::vector<ToolBlockField>& fields, std::vector<std::string>& parts)
		{
			if (parts.empty())
				return;

			std::ostringstream value;
			for (size_t i = 0; i < parts.size(); ++i)
			{
				if (i != 0)
					value << "; ";
				value << parts[i];
			}
			fields.push_back({ "placement", value.str() });
			parts.clear();
		}

		void AppendStructuredPlacementFields(std::string_view header, std::vector<ToolBlockField>& fields)
		{
			bool inPlacementBlock = false;
			size_t placementIndent = 0;
			std::vector<std::string> placementParts;

			size_t lineStart = 0;
			while (lineStart < header.size())
			{
				size_t lineEnd = header.find('\n', lineStart);
				if (lineEnd == std::string_view::npos)
					lineEnd = header.size();

				const std::string_view rawLine = header.substr(lineStart, lineEnd - lineStart);
				const size_t indent = CountLeadingWhitespace(rawLine);
				std::string line = TrimCopy(rawLine);
				if (line.empty())
				{
					lineStart = lineEnd + 1;
					continue;
				}

				const size_t separator = line.find(':');
				const std::string normalizedKey = separator == std::string::npos ? std::string() : NormalizeName(TrimCopy(std::string_view(line).substr(0, separator)));
				if (IsPlacementBlockKey(normalizedKey))
				{
					PushStructuredPlacement(fields, placementParts);
					const std::string value = TrimCopy(std::string_view(line).substr(separator + 1));
					inPlacementBlock = value.empty();
					placementIndent = indent;
					lineStart = lineEnd + 1;
					continue;
				}

				if (!inPlacementBlock)
				{
					lineStart = lineEnd + 1;
					continue;
				}

				if (indent <= placementIndent && !line.starts_with('-') && !line.starts_with('*'))
				{
					PushStructuredPlacement(fields, placementParts);
					inPlacementBlock = false;
					lineStart = lineEnd + 1;
					continue;
				}

				if (line.starts_with('-') || line.starts_with('*'))
				{
					PushStructuredPlacement(fields, placementParts);
					line = TrimCopy(std::string_view(line).substr(1));
					if (line.empty())
					{
						lineStart = lineEnd + 1;
						continue;
					}
				}

				size_t inlineSeparator = line.find('=');
				if (inlineSeparator == std::string::npos)
					inlineSeparator = line.find(':');
				if (inlineSeparator != std::string::npos)
				{
					std::string key = TrimCopy(std::string_view(line).substr(0, inlineSeparator));
					std::string value = TrimCopy(std::string_view(line).substr(inlineSeparator + 1));
					if (!key.empty() && !value.empty() && IsPlacementInlineKey(NormalizeName(key)))
						placementParts.push_back(key + "=" + value);
				}

				lineStart = lineEnd + 1;
			}

			PushStructuredPlacement(fields, placementParts);
		}

		std::vector<ToolBlockField> ParseToolBlockFields(std::string_view header)
		{
			std::vector<ToolBlockField> fields;
			std::string multilineKey;
			size_t lineStart = 0;
			while (lineStart < header.size())
			{
				size_t lineEnd = header.find('\n', lineStart);
				if (lineEnd == std::string_view::npos)
					lineEnd = header.size();

				const std::string line = TrimCopy(header.substr(lineStart, lineEnd - lineStart));
				if (line.empty())
				{
					lineStart = lineEnd + 1;
					continue;
				}

				if (multilineKey == "placement")
				{
					std::string continuation = line;
					if (!continuation.empty() && (continuation.front() == '-' || continuation.front() == '*'))
						continuation = TrimCopy(std::string_view(continuation).substr(1));

					const size_t inlineSeparator = continuation.find_first_of(":=");
					const std::string inlineKey = inlineSeparator == std::string::npos ? std::string() : NormalizeName(TrimCopy(std::string_view(continuation).substr(0, inlineSeparator)));
					const bool looksLikePlacement =
						continuation.find('=') != std::string::npos ||
						inlineKey == "name" ||
						inlineKey == "entity" ||
						inlineKey == "entityname" ||
						inlineKey == "sprite" ||
						inlineKey == "spritename" ||
						inlineKey == "spriteindex" ||
						inlineKey == "assethandle" ||
						inlineKey == "assetid" ||
						inlineKey == "assetpath" ||
						inlineKey == "assetname" ||
						inlineKey == "position" ||
						inlineKey == "translation" ||
						inlineKey == "scale" ||
						inlineKey == "rotation" ||
						inlineKey == "rotationz";
					if (looksLikePlacement)
					{
						fields.push_back({ "placement", continuation });
						lineStart = lineEnd + 1;
						continue;
					}
				}

				const size_t separator = line.find(':');
				if (separator != std::string::npos)
				{
					ToolBlockField field;
					field.m_Key = LowerCopy(TrimCopy(std::string_view(line).substr(0, separator)));
					field.m_Value = TrimCopy(std::string_view(line).substr(separator + 1));
					multilineKey = IsPlacementBlockKey(NormalizeName(field.m_Key)) ? "placement" : std::string();
					fields.push_back(std::move(field));
				}
				else
				{
					multilineKey.clear();
				}

				lineStart = lineEnd + 1;
			}
			AppendStructuredPlacementFields(header, fields);
			return fields;
		}

		std::string GetToolBlockField(const std::vector<ToolBlockField>& fields, std::initializer_list<std::string_view> names)
		{
			for (const ToolBlockField& field : fields)
			{
				for (std::string_view name : names)
				{
					if (field.m_Key == LowerCopy(std::string(name)))
						return field.m_Value;
				}
			}
			return {};
		}

		std::vector<ToolBlockField> ParseInlineFields(std::string_view value)
		{
			std::vector<ToolBlockField> fields;
			size_t partStart = 0;
			while (partStart < value.size())
			{
				size_t partEnd = value.find(';', partStart);
				if (partEnd == std::string_view::npos)
					partEnd = value.size();

				const std::string part = TrimCopy(value.substr(partStart, partEnd - partStart));
				size_t separator = part.find('=');
				if (separator == std::string::npos)
					separator = part.find(':');
				if (separator != std::string::npos)
				{
					ToolBlockField field;
					field.m_Key = NormalizeName(TrimCopy(std::string_view(part).substr(0, separator)));
					field.m_Value = TrimCopy(std::string_view(part).substr(separator + 1));
					fields.push_back(std::move(field));
				}

				partStart = partEnd + 1;
			}
			return fields;
		}

		std::optional<SpriteLevelPlacement> ParseSpriteLevelPlacement(std::string_view value)
		{
			const std::vector<ToolBlockField> fields = ParseInlineFields(value);
			if (fields.empty())
				return std::nullopt;

			SpriteLevelPlacement placement;
			placement.m_EntityName = GetToolBlockField(fields, { "name", "entityname", "entity" });
			placement.m_AssetPath = GetToolBlockField(fields, { "assetpath", "path" });
			placement.m_AssetName = GetToolBlockField(fields, { "assetname", "asset" });
			placement.m_SpriteName = GetToolBlockField(fields, { "spritename", "sprite", "subresource" });

			if (const std::optional<uint64_t> assetHandle = ParseUInt64(GetToolBlockField(fields, { "assethandle", "handle", "assetid" })))
				placement.m_AssetHandle = *assetHandle;
			if (const std::optional<int32_t> spriteIndex = ParseInt32(GetToolBlockField(fields, { "spriteindex", "index", "subresourceindex" })))
				placement.m_SpriteIndex = *spriteIndex;

			glm::vec3 vector;
			if (ParseFlexibleVector3(GetToolBlockField(fields, { "position", "translation", "pos" }), vector))
				placement.m_Translation = vector;
			else
				return std::nullopt;

			if (ParseFlexibleVector3(GetToolBlockField(fields, { "scale", "size" }), vector))
			{
				placement.m_Scale = vector;
				if (placement.m_Scale.z == 0.0f)
					placement.m_Scale.z = 1.0f;
				placement.m_HasScale = true;
			}

			if (const std::optional<float> rotation = ParseFloat(GetToolBlockField(fields, { "rotationz", "rotation", "angle" })))
				placement.m_RotationZ = *rotation;

			if (placement.m_EntityName.empty())
				placement.m_EntityName = placement.m_SpriteName.empty() ? "Level Sprite" : placement.m_SpriteName;
			return placement;
		}

		bool SameSpriteLevelPlacement(const SpriteLevelPlacement& left, const SpriteLevelPlacement& right)
		{
			return left.m_EntityName == right.m_EntityName &&
				left.m_AssetHandle == right.m_AssetHandle &&
				left.m_AssetPath == right.m_AssetPath &&
				left.m_AssetName == right.m_AssetName &&
				left.m_SpriteName == right.m_SpriteName &&
				left.m_SpriteIndex == right.m_SpriteIndex &&
				left.m_Translation == right.m_Translation &&
				left.m_Scale == right.m_Scale &&
				left.m_HasScale == right.m_HasScale &&
				left.m_RotationZ == right.m_RotationZ;
		}

		std::string CanonicalComponentName(std::string value)
		{
			const std::string lower = LowerCopy(value);
			if (lower == "transform" || lower == "transformcomponent")
				return "Transform";
			if (lower == "sprite" || lower == "sprite renderer" || lower == "spriterenderer" || lower == "spriterenderercomponent")
				return "Sprite Renderer";
			if (lower == "circle" || lower == "circle renderer" || lower == "circlerenderer" || lower == "circlerenderercomponent")
				return "Circle Renderer";
			if (lower == "text" || lower == "text renderer" || lower == "textrenderer" || lower == "textcomponent")
				return "Text Renderer";
			if (lower == "camera" || lower == "cameracomponent")
				return "Camera";
			if (lower == "script" || lower == "scriptcomponent")
				return "Script";
			if (lower == "animator" || lower == "animatorcomponent")
				return "Animator";
			if (lower == "rigidbody" || lower == "rigidbody 2d" || lower == "rigidbody2d" || lower == "rigidbody2dcomponent")
				return "Rigidbody2D";
			if (lower == "box collider" || lower == "box collider 2d" || lower == "boxcollider" || lower == "boxcollider2d" || lower == "boxcollider2dcomponent")
				return "BoxCollider2D";
			if (lower == "circle collider" || lower == "circle collider 2d" || lower == "circlecollider" || lower == "circlecollider2d" || lower == "circlecollider2dcomponent")
				return "CircleCollider2D";
			if (lower == "audio" || lower == "audiocomponent")
				return "Audio";
			return value;
		}

		std::optional<ToolProposal> ParseGenericToolBlock(const ContextSnapshot& context, std::string_view block)
		{
			constexpr std::string_view beginMarker = "---BEGIN CONTENT---";
			constexpr std::string_view endMarker = "---END CONTENT---";

			const size_t contentBegin = block.find(beginMarker);
			std::string_view header = contentBegin == std::string_view::npos ? block : block.substr(0, contentBegin);
			std::string content;
			if (contentBegin != std::string_view::npos)
			{
				const size_t contentStart = contentBegin + beginMarker.size();
				const size_t contentEnd = block.find(endMarker, contentStart);
				if (contentEnd == std::string_view::npos)
					return std::nullopt;
				content = StripSingleEdgeNewline(std::string(block.substr(contentStart, contentEnd - contentStart)));
			}

			const std::vector<ToolBlockField> fields = ParseToolBlockFields(header);
			const std::string toolName = GetToolBlockField(fields, { "tool", "kind" });
			const ToolDefinition* definition = FindAssistantTool(toolName);
			if (!definition || !definition->m_ProviderCallable || definition->m_Kind == ToolKind::None)
				return std::nullopt;

			ToolProposal proposal;
			proposal.m_Kind = definition->m_Kind;
			proposal.m_Title = GetToolBlockField(fields, { "title" });
			proposal.m_Description = GetToolBlockField(fields, { "summary", "description" });
			if (proposal.m_Title.empty())
				proposal.m_Title = definition->m_DisplayName;
			if (proposal.m_Description.empty())
				proposal.m_Description = definition->m_Description;

			if (const std::optional<uint64_t> targetEntity = ParseUInt64(GetToolBlockField(fields, { "targetentity", "entityid", "target" })))
				proposal.m_TargetEntity = *targetEntity;
			else
				proposal.m_TargetEntity = context.m_SelectedEntity;

			switch (definition->m_Kind)
			{
			case ToolKind::CreateEntity:
			{
				proposal.m_EntityName = GetToolBlockField(fields, { "entityname", "entity", "name" });
				if (proposal.m_EntityName.empty())
					proposal.m_EntityName = "AI Entity";
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Create " + proposal.m_EntityName;

				glm::vec3 vector;
				if (ParseVector3(GetToolBlockField(fields, { "translation", "position" }), vector))
				{
					proposal.m_Translation = vector;
					proposal.m_HasTransform = true;
				}
				if (ParseVector3(GetToolBlockField(fields, { "rotation" }), vector))
				{
					proposal.m_Rotation = vector;
					proposal.m_HasTransform = true;
				}
				if (ParseVector3(GetToolBlockField(fields, { "scale" }), vector))
				{
					proposal.m_Scale = vector;
					proposal.m_HasTransform = true;
				}
				return proposal;
			}
			case ToolKind::AddComponent:
				proposal.m_ComponentName = CanonicalComponentName(GetToolBlockField(fields, { "componentname", "component", "type" }));
				if (proposal.m_ComponentName.empty())
					return std::nullopt;
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Add " + proposal.m_ComponentName;
				return proposal;
			case ToolKind::SetTransform:
			{
				glm::vec3 translation;
				glm::vec3 rotation;
				glm::vec3 scale;
				if (!ParseVector3(GetToolBlockField(fields, { "translation", "position" }), translation) ||
					!ParseVector3(GetToolBlockField(fields, { "rotation" }), rotation) ||
					!ParseVector3(GetToolBlockField(fields, { "scale" }), scale))
				{
					return std::nullopt;
				}

				proposal.m_Translation = translation;
				proposal.m_Rotation = rotation;
				proposal.m_Scale = scale;
				proposal.m_HasTransform = true;
				return proposal;
			}
			case ToolKind::EditComponent:
			{
				proposal.m_ComponentName = CanonicalComponentName(GetToolBlockField(fields, { "componentname", "component", "type" }));
				if (proposal.m_ComponentName.empty())
					return std::nullopt;

				for (const ToolBlockField& field : fields)
				{
					if (!field.m_Key.starts_with("field."))
						continue;

					ComponentFieldEdit edit;
					edit.m_FieldName = TrimCopy(std::string_view(field.m_Key).substr(6));
					edit.m_Value = field.m_Value;
					if (!edit.m_FieldName.empty() && !edit.m_Value.empty())
						proposal.m_ComponentFields.push_back(std::move(edit));
				}

				const std::string singleField = GetToolBlockField(fields, { "field", "fieldname", "property", "propertyname" });
				const std::string singleValue = GetToolBlockField(fields, { "value", "fieldvalue", "propertyvalue" });
				if (!singleField.empty() && !singleValue.empty())
					proposal.m_ComponentFields.push_back({ singleField, singleValue });

				if (proposal.m_ComponentFields.empty())
					return std::nullopt;
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Edit " + proposal.m_ComponentName;
				return proposal;
			}
			case ToolKind::AssetOperation:
			{
				const std::string operation = NormalizeName(GetToolBlockField(fields, { "operation", "assetoperation" }));
				if (!operation.empty() && operation != "assignasset" && operation != "assign")
					return std::nullopt;
				proposal.m_AssetOperation = "assign_asset";

				proposal.m_ComponentName = CanonicalComponentName(GetToolBlockField(fields, { "componentname", "component", "type" }));
				proposal.m_AssetField = GetToolBlockField(fields, { "field", "assetfield", "property", "propertyname" });
				proposal.m_AssetPath = GetToolBlockField(fields, { "assetpath", "path" });
				proposal.m_AssetName = GetToolBlockField(fields, { "assetname", "name" });
				proposal.m_AssetSubresource = GetToolBlockField(fields, { "spritename", "subresource", "subresourcename" });
				proposal.m_AssetType = ParseAssetType(GetToolBlockField(fields, { "assettype", "typehint" }));

				if (const std::optional<uint64_t> assetHandle = ParseUInt64(GetToolBlockField(fields, { "assethandle", "handle", "assetid" })))
					proposal.m_AssetHandle = *assetHandle;
				if (const std::optional<int32_t> spriteIndex = ParseInt32(GetToolBlockField(fields, { "spriteindex", "subresourceindex" })))
					proposal.m_AssetSubresourceIndex = *spriteIndex;

				if (proposal.m_ComponentName.empty() || proposal.m_AssetField.empty() ||
					(proposal.m_AssetHandle == 0 && proposal.m_AssetPath.empty() && proposal.m_AssetName.empty()))
				{
					return std::nullopt;
				}
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Assign asset to " + proposal.m_ComponentName;
				return proposal;
			}
			case ToolKind::CreateSpriteLevel:
			{
				proposal.m_AssetPath = GetToolBlockField(fields, { "assetpath", "path" });
				proposal.m_AssetName = GetToolBlockField(fields, { "assetname", "name" });
				proposal.m_AssetType = AssetType::Texture2D;
				if (const std::optional<uint64_t> assetHandle = ParseUInt64(GetToolBlockField(fields, { "assethandle", "handle", "assetid" })))
					proposal.m_AssetHandle = *assetHandle;

				for (const ToolBlockField& field : fields)
				{
					if (!IsPlacementBlockKey(NormalizeName(field.m_Key)))
						continue;
					if (std::optional<SpriteLevelPlacement> placement = ParseSpriteLevelPlacement(field.m_Value))
					{
						const bool duplicate = std::ranges::any_of(proposal.m_LevelPlacements,
							[&placement](const SpriteLevelPlacement& existing)
							{
								return SameSpriteLevelPlacement(existing, *placement);
							});
						if (!duplicate)
							proposal.m_LevelPlacements.push_back(std::move(*placement));
					}
				}

				const bool hasPlacementAsset = std::ranges::any_of(proposal.m_LevelPlacements,
					[](const SpriteLevelPlacement& placement)
					{
						return placement.m_AssetHandle != 0 || !placement.m_AssetPath.empty() || !placement.m_AssetName.empty();
					});
				if ((proposal.m_AssetHandle == 0 && proposal.m_AssetPath.empty() && proposal.m_AssetName.empty() && !hasPlacementAsset) || proposal.m_LevelPlacements.empty())
					return std::nullopt;
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Create sprite level";
				return proposal;
			}
			case ToolKind::EditScript:
				proposal.m_ScriptPath = GetToolBlockField(fields, { "path", "scriptpath" });
				if (proposal.m_ScriptPath.empty())
					proposal.m_ScriptPath = context.m_SelectedScriptPath;
				if (proposal.m_ScriptPath.empty() || content.empty())
					return std::nullopt;
				proposal.m_ScriptContent = std::move(content);
				if (proposal.m_Title == definition->m_DisplayName)
					proposal.m_Title = "Edit " + std::filesystem::path(proposal.m_ScriptPath).filename().string();
				return proposal;
			case ToolKind::None:
			default:
				return std::nullopt;
			}
		}

		std::optional<ToolProposal> ParseLegacyScriptEditBlock(const ContextSnapshot& context, std::string_view block)
		{
			constexpr std::string_view beginMarker = "---BEGIN CONTENT---";
			constexpr std::string_view endMarker = "---END CONTENT---";

			const size_t contentBegin = block.find(beginMarker);
			if (contentBegin == std::string_view::npos)
				return std::nullopt;

			const size_t contentStart = contentBegin + beginMarker.size();
			const size_t contentEnd = block.find(endMarker, contentStart);
			if (contentEnd == std::string_view::npos)
				return std::nullopt;

			const std::string header = std::string(block.substr(0, contentBegin));
			std::string scriptPath = context.m_SelectedScriptPath;
			std::string summary = "Assistant script edit";

			size_t lineStart = 0;
			while (lineStart < header.size())
			{
				size_t lineEnd = header.find('\n', lineStart);
				if (lineEnd == std::string::npos)
					lineEnd = header.size();

				const std::string line = TrimCopy(std::string_view(header).substr(lineStart, lineEnd - lineStart));
				if (StartsWithNoCase(line, "path:"))
					scriptPath = TrimCopy(std::string_view(line).substr(5));
				else if (StartsWithNoCase(line, "summary:"))
					summary = TrimCopy(std::string_view(line).substr(8));

				lineStart = lineEnd + 1;
			}

			std::string scriptContent = StripSingleEdgeNewline(std::string(block.substr(contentStart, contentEnd - contentStart)));
			if (scriptPath.empty() || scriptContent.empty())
				return std::nullopt;

			ToolProposal proposal;
			proposal.m_Kind = ToolKind::EditScript;
			proposal.m_Title = "Edit " + std::filesystem::path(scriptPath).filename().string();
			proposal.m_Description = summary.empty() ? "Replace selected script source with assistant generated code." : summary;
			proposal.m_TargetEntity = context.m_SelectedEntity;
			proposal.m_ScriptPath = std::move(scriptPath);
			proposal.m_ScriptContent = std::move(scriptContent);
			return proposal;
		}

		template<typename Parser>
		void ParseFencedToolBlocks(std::vector<ToolProposal>& proposals, const ContextSnapshot& context, const std::string& responseText, std::string_view fence, Parser parser)
		{
			size_t searchOffset = 0;
			while (searchOffset < responseText.size())
			{
				const size_t fenceStart = responseText.find(fence, searchOffset);
				if (fenceStart == std::string::npos)
					break;

				const size_t blockStart = responseText.find('\n', fenceStart + fence.size());
				if (blockStart == std::string::npos)
					break;

				const size_t fenceEnd = responseText.find("```", blockStart + 1);
				if (fenceEnd == std::string::npos)
					break;

				if (std::optional<ToolProposal> proposal = parser(context, std::string_view(responseText).substr(blockStart + 1, fenceEnd - blockStart - 1)))
					proposals.push_back(std::move(*proposal));

				searchOffset = fenceEnd + 3;
			}
		}

		void StripFencedToolBlocks(std::string& text, std::string_view fence)
		{
			size_t copyOffset = 0;
			std::string result;

			while (copyOffset < text.size())
			{
				const size_t fenceStart = text.find(fence, copyOffset);
				if (fenceStart == std::string::npos)
				{
					result.append(text.substr(copyOffset));
					break;
				}

				result.append(text.substr(copyOffset, fenceStart - copyOffset));
				const size_t blockStart = text.find('\n', fenceStart + fence.size());
				if (blockStart == std::string::npos)
					break;

				const size_t fenceEnd = text.find("```", blockStart + 1);
				if (fenceEnd == std::string::npos)
					break;

				copyOffset = fenceEnd + 3;
			}

			text = std::move(result);
		}

		std::string PickEntityName(const std::string& prompt)
		{
			const size_t firstQuote = prompt.find_first_of("\"'");
			if (firstQuote != std::string::npos)
			{
				const size_t secondQuote = prompt.find_first_of("\"'", firstQuote + 1);
				if (secondQuote != std::string::npos && secondQuote > firstQuote + 1)
					return prompt.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			}

			const std::string lower = LowerCopy(prompt);
			if (ContainsAny(lower, { "camera", "kamera" }))
				return "AI Camera";
			if (ContainsAny(lower, { "player", "character", "karakter", "oyuncu" }))
				return "Player";
			if (ContainsAny(lower, { "enemy", "dusman", "dushman" }))
				return "Enemy";
			return "AI Entity";
		}

		void PushAddComponentProposal(std::vector<ToolProposal>& proposals, const ContextSnapshot& context, std::string componentName)
		{
			if (!context.m_HasSelection)
				return;

			ToolProposal proposal;
			proposal.m_Kind = ToolKind::AddComponent;
			proposal.m_Title = "Add " + componentName;
			proposal.m_Description = "Adds " + componentName + " to selected entity '" + context.m_SelectedEntityName + "'.";
			proposal.m_TargetEntity = context.m_SelectedEntity;
			proposal.m_ComponentName = std::move(componentName);
			proposals.push_back(std::move(proposal));
		}
	}

	const char* RoleName(Role role)
	{
		switch (role)
		{
		case Role::User: return "User";
		case Role::Assistant: return "Assistant";
		case Role::System: return "System";
		default: return "Unknown";
		}
	}

	const char* ToolKindName(ToolKind kind)
	{
		if (const ToolDefinition* definition = FindAssistantTool(kind))
			return definition->m_DisplayName.c_str();
		return "None";
	}

	const char* ProviderName(ProviderKind provider)
	{
		switch (provider)
		{
		case ProviderKind::OpenAI: return "openai";
		case ProviderKind::Gemini: return "gemini";
		case ProviderKind::Offline:
		default: return "offline";
		}
	}

	const char* ProviderDisplayName(ProviderKind provider)
	{
		switch (provider)
		{
		case ProviderKind::OpenAI: return "OpenAI";
		case ProviderKind::Gemini: return "Gemini";
		case ProviderKind::Offline:
		default: return "Offline";
		}
	}

	ProviderKind ProviderFromName(const std::string& name)
	{
		const std::string lower = LowerCopy(name);
		if (lower == "openai")
			return ProviderKind::OpenAI;
		if (lower == "gemini")
			return ProviderKind::Gemini;
		return ProviderKind::Offline;
	}

	const char* ApplyModeName(ApplyMode mode)
	{
		switch (mode)
		{
		case ApplyMode::Review: return "review";
		case ApplyMode::AutoApplySafe: return "auto_safe";
		case ApplyMode::AutoApplyAll: return "auto_all";
		default: return "review";
		}
	}

	const char* ApplyModeDisplayName(ApplyMode mode)
	{
		switch (mode)
		{
		case ApplyMode::Review: return "Review";
		case ApplyMode::AutoApplySafe: return "Auto Safe";
		case ApplyMode::AutoApplyAll: return "Auto All";
		default: return "Review";
		}
	}

	const char* ApplyModeDescription(ApplyMode mode)
	{
		switch (mode)
		{
		case ApplyMode::Review: return "Queue every proposal until you apply it.";
		case ApplyMode::AutoApplySafe: return "Automatically apply undo-friendly scene and asset proposals; queue script edits.";
		case ApplyMode::AutoApplyAll: return "Automatically apply every proposal, including script edits.";
		default: return "Queue every proposal until you apply it.";
		}
	}

	ApplyMode ApplyModeFromName(const std::string& name)
	{
		const std::string lower = LowerCopy(name);
		if (lower == "auto_safe" || lower == "autosafe" || lower == "safe")
			return ApplyMode::AutoApplySafe;
		if (lower == "auto_all" || lower == "autoall" || lower == "all")
			return ApplyMode::AutoApplyAll;
		return ApplyMode::Review;
	}

	bool HasProviderCredentials(const Settings& settings)
	{
		if (AssistantProviderPtr provider = CreateAssistantProvider(settings.m_Provider))
			return provider->HasCredentials(settings);
		return false;
	}

	std::string BuildContextPrompt(const ContextSnapshot& context, const Settings& settings)
	{
		std::ostringstream stream;
		stream << "Whip Editor context:\n";
		stream << "- Local editor time: " << ProviderUtils::GetLocalEditorTime() << '\n';
		stream << "- Project: " << (context.m_HasProject ? context.m_ProjectName : "none") << '\n';
		stream << "- Scene: " << (context.m_HasScene ? context.m_ScenePath : "none") << '\n';

		if (settings.m_SendSceneContext && context.m_HasSelection)
		{
			stream << "- Selected entity: " << context.m_SelectedEntityName << " (" << context.m_SelectedEntity << ")\n";
			stream << "- Components:";
			for (const std::string& component : context.m_SelectedComponents)
				stream << ' ' << component;
			stream << '\n';

			if (context.m_HasSelectedScript)
				stream << "- Selected script class: " << context.m_SelectedScriptClass << '\n';

			if (context.m_HasSelectedScriptSource)
			{
				stream << "- Selected script path: " << context.m_SelectedScriptPath << '\n';
				stream << "- Selected script source:\n```csharp\n";
				stream << context.m_SelectedScriptSource;
				if (!context.m_SelectedScriptSource.empty() && context.m_SelectedScriptSource.back() != '\n')
					stream << '\n';
				stream << "```\n";
			}
		}

		if (settings.m_SendConsoleContext && !context.m_RecentConsole.empty())
		{
			stream << "- Recent console:\n";
			for (const std::string& line : context.m_RecentConsole)
				stream << "  " << line << '\n';
		}

		if (!context.m_ProjectAssets.empty())
		{
			stream << "- Project assets available for asset_operation and create_sprite_level. Prefer assetHandle over name/path when emitting a tool block:\n";
			for (const ContextSnapshot::AssetSummary& asset : context.m_ProjectAssets)
			{
				stream << "  - handle: " << asset.m_Handle << ", type: " << AssetTypeName(asset.m_Type) << ", path: " << asset.m_Path;
				if (!asset.m_Name.empty())
					stream << ", name: " << asset.m_Name;
				if (asset.m_SpriteCount > 0)
					stream << ", spriteCount: " << asset.m_SpriteCount;
				if (!asset.m_Sprites.empty())
				{
					stream << ", sprites:";
					for (size_t i = 0; i < asset.m_Sprites.size(); ++i)
					{
						stream << " [" << i << "] " << asset.m_Sprites[i];
						if (i < asset.m_SpriteDetails.size())
						{
							const ContextSnapshot::SpriteSummary& sprite = asset.m_SpriteDetails[i];
							stream << " rect=" << sprite.m_X << ',' << sprite.m_Y << ',' << sprite.m_Width << 'x' << sprite.m_Height;
							if (sprite.m_Width > sprite.m_Height * 2)
								stream << " wide";
							else if (sprite.m_Height > sprite.m_Width * 2)
								stream << " tall";
							else if (sprite.m_Width > sprite.m_Height)
								stream << " landscape";
							else if (sprite.m_Height > sprite.m_Width)
								stream << " portrait";
							else
								stream << " square";
						}
						stream << ';';
					}
				}
				stream << '\n';
			}
			if (settings.m_SendAssetImages)
				stream << "- Gemini may receive matching texture files as image attachments. Use the attached atlas image plus sprite rect indices to choose sensible sprites.\n";
		}

		if (settings.m_SendSceneContext && !context.m_SceneEntities.empty())
		{
			stream << "- Scene entities for scale/reference. Respect these when placing new level pieces:\n";
			for (const ContextSnapshot::EntitySummary& entity : context.m_SceneEntities)
			{
				stream << "  - " << entity.m_Name << " (" << entity.m_Id << ")";
				if (!entity.m_Components.empty())
				{
					stream << " components:";
					for (const std::string& component : entity.m_Components)
						stream << ' ' << component;
				}
				if (entity.m_HasTransform)
					stream << " pos=" << FormatVec3(entity.m_Translation) << " scale=" << FormatVec3(entity.m_Scale);
				if (entity.m_TextureHandle != 0)
				{
					stream << " textureHandle=" << entity.m_TextureHandle;
					if (!entity.m_TexturePath.empty())
						stream << " texturePath=" << entity.m_TexturePath;
					if (entity.m_TextureSpriteIndex >= 0)
						stream << " spriteIndex=" << entity.m_TextureSpriteIndex;
					if (!entity.m_SpriteName.empty())
						stream << " spriteName=" << entity.m_SpriteName;
				}
				stream << '\n';
			}
		}

		stream << "- Level design quality rules for create_sprite_level: use existing entity scale as reference, build a playable composition with a start, route, platforms, gaps, landmarks, and decoration when assets exist. Do not create only a flat repeated block grid unless the user explicitly asks for a test grid. For normal level requests emit at least 18 placement lines; for detailed/big level requests emit 30-80 placement lines. Every prop, landmark, platform, chest, tree, bush, crate, torch, or decoration mentioned in your summary must have a matching placement line. Use varied sprite indices and sprite roles; when mixing texture assets, include assetHandle on each placement line. Keep solid ground/support pieces aligned, place props on top of support surfaces, and avoid overlapping the player start.\n";

		stream << "\n" << ProviderUtils::BuildWhipScriptingGuide();
		stream << "\nAnswer as a concise game-engine assistant. When scene or code changes are needed, emit provider-callable tool blocks and then add a short human summary. Prefer one larger proposal over many tiny proposals when the operation is naturally one user action.";
		return stream.str();
	}

	std::vector<ToolProposal> BuildLocalProposals(const ContextSnapshot& context, const std::string& prompt)
	{
		std::vector<ToolProposal> proposals;
		if (!context.m_HasScene)
			return proposals;

		const std::string lower = LowerCopy(prompt);
		const bool wantsCreate = ContainsAny(lower, { "create", "add entity", "new entity", "olustur", "ekle", "nesne", "obje" });
		if (wantsCreate)
		{
			ToolProposal proposal;
			proposal.m_Kind = ToolKind::CreateEntity;
			proposal.m_EntityName = PickEntityName(prompt);
			proposal.m_Title = "Create " + proposal.m_EntityName;
			proposal.m_Description = "Creates a new entity in the active edit scene and selects it.";
			if (ContainsAny(lower, { "camera", "kamera" }))
				proposal.m_Translation = { 0.0f, 0.0f, 8.0f };
			proposal.m_HasTransform = ContainsAny(lower, { "camera", "kamera", "at ", "position", "konum" });
			proposals.push_back(std::move(proposal));
		}

		if (ContainsAny(lower, { "sprite", "texture", "2d render", "render" }))
			PushAddComponentProposal(proposals, context, "Sprite Renderer");
		if (ContainsAny(lower, { "circle", "daire" }))
			PushAddComponentProposal(proposals, context, "Circle Renderer");
		if (ContainsAny(lower, { "text", "font", "yazi" }))
			PushAddComponentProposal(proposals, context, "Text Renderer");
		if (ContainsAny(lower, { "camera", "kamera" }))
			PushAddComponentProposal(proposals, context, "Camera");
		if (ContainsAny(lower, { "script", "cs", "mono" }))
			PushAddComponentProposal(proposals, context, "Script");
		if (ContainsAny(lower, { "animator", "animation", "animasyon" }))
			PushAddComponentProposal(proposals, context, "Animator");
		if (ContainsAny(lower, { "rigidbody", "physics", "fizik", "dynamic" }))
			PushAddComponentProposal(proposals, context, "Rigidbody2D");
		if (ContainsAny(lower, { "box collider", "boxcollider", "kutu collider" }))
			PushAddComponentProposal(proposals, context, "BoxCollider2D");
		if (ContainsAny(lower, { "circle collider", "circlecollider", "daire collider" }))
			PushAddComponentProposal(proposals, context, "CircleCollider2D");
		if (ContainsAny(lower, { "audio", "sound", "ses" }))
			PushAddComponentProposal(proposals, context, "Audio");

		return proposals;
	}

	std::vector<ToolProposal> ParseToolProposals(const ContextSnapshot& context, const std::string& responseText)
	{
		std::vector<ToolProposal> proposals;
		ParseFencedToolBlocks(proposals, context, responseText, "```whip_tool", ParseGenericToolBlock);
		ParseFencedToolBlocks(proposals, context, responseText, "```whip_script_edit", ParseLegacyScriptEditBlock);

		return proposals;
	}

	std::string StripToolProposalBlocks(const std::string& responseText)
	{
		std::string result = responseText;
		StripFencedToolBlocks(result, "```whip_tool");
		StripFencedToolBlocks(result, "```whip_script_edit");
		return TrimCopy(result);
	}

	Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		if (settings.m_Provider != ProviderKind::Offline)
		{
			if (AssistantProviderPtr provider = CreateAssistantProvider(settings.m_Provider))
				return provider->RequestResponse(settings, context, prompt);
		}

		Response response;
		response.m_Success = true;
		response.m_Text = "Offline provider created local proposals only.";
		response.m_Proposals = BuildLocalProposals(context, prompt);
		return response;
	}

	Response RequestOpenAIResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		OpenAIProvider provider;
		return provider.RequestResponse(settings, context, prompt);
	}

	Response RequestGeminiResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		GeminiProvider provider;
		return provider.RequestResponse(settings, context, prompt);
	}
}

_WHIP_END
