#include <Whip-Assistant/AssistantToolRegistry.h>

#include <algorithm>
#include <cctype>
#include <sstream>

_WHIP_START

namespace Assistant
{
	namespace
	{
		std::string EscapeJson(const std::string& value)
		{
			std::string result;
			result.reserve(value.size() + 16);
			for (const char character : value)
			{
				switch (character)
				{
				case '\\': result += "\\\\"; break;
				case '"': result += "\\\""; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				default: result += character; break;
				}
			}
			return result;
		}

		std::string LowerCopy(std::string_view value)
		{
			std::string result(value);
			std::ranges::transform(result, result.begin(),
				[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
			return result;
		}

		std::string NormalizeToolName(std::string_view value)
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

		const char* ToolKindToken(ToolKind kind)
		{
			switch (kind)
			{
			case ToolKind::CreateEntity: return "createEntity";
			case ToolKind::AddComponent: return "addComponent";
			case ToolKind::SetTransform: return "setTransform";
			case ToolKind::EditComponent: return "editComponent";
			case ToolKind::EditScript: return "editScript";
			case ToolKind::None:
			default: return "none";
			}
		}
	}

	const std::vector<ToolDefinition>& GetAssistantToolRegistry()
	{
		static const std::vector<ToolDefinition> s_Registry =
		{
			{
				.m_Kind = ToolKind::CreateEntity,
				.m_Name = "create_entity",
				.m_DisplayName = "Create Entity",
				.m_Status = "available",
				.m_Description = "Creates a new entity in the active edit scene.",
				.m_ResponseFormat = "Return a ```whip_tool block with tool: create_entity and entityName. Optional transform fields are translation, rotation, and scale as x,y,z.",
				.m_ProviderCallable = true,
				.m_Fields =
				{
					{ "entityName", "string", false, "Suggested entity name." },
					{ "translation", "Vector3", false, "Optional initial world position." },
					{ "rotation", "Vector3", false, "Optional initial Euler rotation." },
					{ "scale", "Vector3", false, "Optional initial scale." }
				}
			},
			{
				.m_Kind = ToolKind::AddComponent,
				.m_Name = "add_component",
				.m_DisplayName = "Add Component",
				.m_Status = "available",
				.m_Description = "Adds a supported component to the selected entity.",
				.m_ResponseFormat = "Return a ```whip_tool block with tool: add_component and componentName. Omit targetEntity to use the current selection.",
				.m_ProviderCallable = true,
				.m_Fields =
				{
					{ "targetEntity", "EntityId", false, "Selected entity id when available." },
					{ "componentName", "string", true, "Supported component display name, such as Sprite Renderer, Script, Animator, Rigidbody2D, BoxCollider2D, or Audio." }
				}
			},
			{
				.m_Kind = ToolKind::SetTransform,
				.m_Name = "set_transform",
				.m_DisplayName = "Set Transform",
				.m_Status = "available",
				.m_Description = "Updates transform values on the target entity.",
				.m_ResponseFormat = "Return a ```whip_tool block with tool: set_transform, translation, rotation, and scale. All three vector fields are required to avoid resetting unknown values.",
				.m_ProviderCallable = true,
				.m_Fields =
				{
					{ "targetEntity", "EntityId", false, "Selected entity id when available." },
					{ "translation", "Vector3", false, "Optional world position." },
					{ "rotation", "Vector3", false, "Optional Euler rotation." },
					{ "scale", "Vector3", false, "Optional scale." }
				}
			},
			{
				.m_Kind = ToolKind::EditComponent,
				.m_Name = "edit_component",
				.m_DisplayName = "Edit Component",
				.m_Status = "available",
				.m_Description = "Changes safe serialized fields on a component attached to the selected entity.",
				.m_ResponseFormat = "Return a ```whip_tool block with tool: edit_component, componentName, and one or more field.<FieldName>: <value> lines. Omit targetEntity to use the current selection.",
				.m_ProviderCallable = true,
				.m_Fields =
				{
					{ "targetEntity", "EntityId", false, "Selected entity id when available." },
					{ "componentName", "string", true, "Supported component display name, such as Transform, Sprite Renderer, Text Renderer, Camera, Rigidbody2D, BoxCollider2D, CircleCollider2D, Script, or Animator." },
					{ "field.<FieldName>", "string/number/bool/vector", true, "Field edit. Examples: field.Color: 1,1,1,1, field.GravityScale: 2.0, field.Text: Hello." }
				}
			},
			{
				.m_Kind = ToolKind::EditScript,
				.m_Name = "edit_script",
				.m_DisplayName = "Edit Script",
				.m_Status = "available",
				.m_Description = "Proposes a complete replacement for an existing C# script under Assets/Scripts. The editor applies it through the undo-friendly proposal UI.",
				.m_ResponseFormat = "Return exactly one ```whip_tool block with tool: edit_script, path, summary, ---BEGIN CONTENT---, complete C# file, ---END CONTENT---.",
				.m_ProviderCallable = true,
				.m_Fields =
				{
					{ "path", "project-relative path", true, "Selected script path under Assets/Scripts." },
					{ "summary", "string", true, "Short user-facing summary of the change." },
					{ "content", "C# source", true, "Complete replacement file content." }
				}
			},
			{
				.m_Kind = ToolKind::None,
				.m_Name = "edit_animation_controller",
				.m_DisplayName = "Edit Animation Controller",
				.m_Status = "planned",
				.m_Description = "Will modify animation controller state graphs, transition blueprints, parameters, and clips.",
				.m_ResponseFormat = "Not callable yet.",
				.m_ProviderCallable = false
			},
			{
				.m_Kind = ToolKind::None,
				.m_Name = "asset_operation",
				.m_DisplayName = "Asset Operation",
				.m_Status = "planned",
				.m_Description = "Will import, create, find, slice, edit, or delete project assets.",
				.m_ResponseFormat = "Not callable yet.",
				.m_ProviderCallable = false
			},
			{
				.m_Kind = ToolKind::None,
				.m_Name = "run_build_or_validation",
				.m_DisplayName = "Run Build Or Validation",
				.m_Status = "planned",
				.m_Description = "Will trigger script build, project validation, and diagnostic flows.",
				.m_ResponseFormat = "Not callable yet.",
				.m_ProviderCallable = false
			}
		};

		return s_Registry;
	}

	const ToolDefinition* FindAssistantTool(ToolKind kind)
	{
		if (kind == ToolKind::None)
			return nullptr;

		const std::vector<ToolDefinition>& registry = GetAssistantToolRegistry();
		const auto iterator = std::ranges::find_if(registry,
			[kind](const ToolDefinition& definition) { return definition.m_Kind == kind; });
		return iterator == registry.end() ? nullptr : &(*iterator);
	}

	const ToolDefinition* FindAssistantTool(std::string_view name)
	{
		const std::string needle = LowerCopy(name);
		const std::string normalizedNeedle = NormalizeToolName(name);
		const std::vector<ToolDefinition>& registry = GetAssistantToolRegistry();
		const auto iterator = std::ranges::find_if(registry,
			[&needle, &normalizedNeedle](const ToolDefinition& definition)
			{
				return LowerCopy(definition.m_Name) == needle ||
					LowerCopy(definition.m_DisplayName) == needle ||
					NormalizeToolName(definition.m_Name) == normalizedNeedle ||
					NormalizeToolName(definition.m_DisplayName) == normalizedNeedle;
			});
		return iterator == registry.end() ? nullptr : &(*iterator);
	}

	std::string BuildAssistantToolRegistryPrompt()
	{
		std::ostringstream stream;
		stream << "Assistant proposal tools:\n";
		stream << "- Emit provider-callable tools as fenced ```whip_tool blocks, one block per applyable action.\n";
		stream << "- Generic block header format is one key/value per line: tool: <name>, title: <short title>, summary: <short summary>, plus the fields listed below.\n";
		stream << "- Script edits put complete file contents between ---BEGIN CONTENT--- and ---END CONTENT--- inside the same block.\n";
		stream << "- Planned tools are roadmap knowledge only and must not be emitted as tool blocks.\n";
		stream << "- All available tools still require explicit user/editor apply before mutating the project.\n";

		for (const ToolDefinition& tool : GetAssistantToolRegistry())
		{
			stream << "- " << tool.m_DisplayName << " (`" << tool.m_Name << "`) [" << tool.m_Status << "]";
			stream << " provider-callable: " << (tool.m_ProviderCallable ? "yes" : "no") << " - " << tool.m_Description << '\n';

			if (!tool.m_ResponseFormat.empty())
				stream << "  Response format: " << tool.m_ResponseFormat << '\n';

			if (!tool.m_Fields.empty())
			{
				stream << "  Fields:\n";
				for (const ToolFieldDefinition& field : tool.m_Fields)
				{
					stream << "    - " << field.m_Name << " : " << field.m_Type;
					stream << (field.m_Required ? " required" : " optional");
					if (!field.m_Description.empty())
						stream << " - " << field.m_Description;
					stream << '\n';
				}
			}
		}

		return stream.str();
	}

	std::string BuildAssistantToolRegistryJson()
	{
		std::ostringstream stream;
		stream << "{\"tools\":[";
		const std::vector<ToolDefinition>& registry = GetAssistantToolRegistry();
		for (size_t i = 0; i < registry.size(); ++i)
		{
			if (i != 0)
				stream << ',';

			const ToolDefinition& tool = registry[i];
			stream << "{";
			stream << "\"kind\":\"" << ToolKindToken(tool.m_Kind) << "\",";
			stream << "\"name\":\"" << EscapeJson(tool.m_Name) << "\",";
			stream << "\"displayName\":\"" << EscapeJson(tool.m_DisplayName) << "\",";
			stream << "\"status\":\"" << EscapeJson(tool.m_Status) << "\",";
			stream << "\"description\":\"" << EscapeJson(tool.m_Description) << "\",";
			stream << "\"responseFormat\":\"" << EscapeJson(tool.m_ResponseFormat) << "\",";
			stream << "\"providerCallable\":" << (tool.m_ProviderCallable ? "true" : "false") << ',';
			stream << "\"requiresApply\":" << (tool.m_RequiresApply ? "true" : "false") << ',';
			stream << "\"fields\":[";
			for (size_t fieldIndex = 0; fieldIndex < tool.m_Fields.size(); ++fieldIndex)
			{
				if (fieldIndex != 0)
					stream << ',';

				const ToolFieldDefinition& field = tool.m_Fields[fieldIndex];
				stream << "{";
				stream << "\"name\":\"" << EscapeJson(field.m_Name) << "\",";
				stream << "\"type\":\"" << EscapeJson(field.m_Type) << "\",";
				stream << "\"required\":" << (field.m_Required ? "true" : "false") << ',';
				stream << "\"description\":\"" << EscapeJson(field.m_Description) << "\"";
				stream << "}";
			}
			stream << "]}";
		}
		stream << "]}";
		return stream.str();
	}
}

_WHIP_END
