#include <Whip-Editor/Helpers/ScriptFieldHelper.h>

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/Project/Project.h>
#include <Whip-Editor/UI/UIHelpers.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

_WHIP_START

namespace
{
	constexpr const char* SceneEntityPayloadType = "WHIP_SCENE_ENTITY";

	class TableRowScope
	{
	public:
		TableRowScope(bool active, const char* label)
			: m_Active(active)
		{
			if (!m_Active)
				return;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-1.0f);
		}

		~TableRowScope()
		{
			if (m_Active)
				ImGui::PopItemWidth();
		}

		TableRowScope(const TableRowScope&) = delete;
		TableRowScope& operator=(const TableRowScope&) = delete;

	private:
		bool m_Active = false;
	};

	std::string ControlLabel(const ScriptField& field, bool inTable)
	{
		if (!inTable)
			return field.m_Name;

		return "##" + field.m_Name;
	}

	std::string ArrayControlLabel(const ScriptField& field, std::string_view rowLabel)
	{
		std::string label = "##";
		label += field.m_Name;
		label += rowLabel;
		return label;
	}

	std::string ArrayTableId(const ScriptField& field)
	{
		return "ArrayTable##" + field.m_Name;
	}

	std::string ArrayRowLabel(size_t index)
	{
		return "[" + std::to_string(index) + "]";
	}

	size_t SanitizeArraySize(int size)
	{
		if (size <= 0)
			return 0;

		return static_cast<size_t>(size);
	}

	ScriptFieldInstance& EditorField(Entity entity, const ScriptField& field)
	{
		auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
		return entityFields.at(field.m_Name);
	}

	ScriptFieldInstance& BaseField(const std::string& className, const ScriptField& field)
	{
		auto& baseEntityFields = ScriptEngine::GetBaseScriptFieldMap(className);
		return baseEntityFields.at(field.m_Name);
	}

	ScriptFieldInstance& OverrideField(Entity entity, const ScriptField& field)
	{
		auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
		ScriptFieldInstance& fieldInstance = entityFields[field.m_Name];
		fieldInstance.m_Field = field;
		return fieldInstance;
	}

	bool DrawFloatControl(const char* id, float& value)
	{
		return ImGui::DragFloat(id, &value);
	}

	bool DrawIntControl(const char* id, int& value)
	{
		return ImGui::InputInt(id, &value);
	}

	bool DrawBoolControl(const char* id, bool& value)
	{
		return ImGui::Checkbox(id, &value);
	}

	template <ImGuiDataType DataType, typename T>
	bool DrawScalarControl(const char* id, T& value)
	{
		return ImGui::InputScalar(id, DataType, &value);
	}

	bool DrawLongControl(const char* id, int64_t& value)
	{
		return DrawScalarControl<ImGuiDataType_S64>(id, value);
	}

	bool DrawUintControl(const char* id, uint32_t& value)
	{
		return DrawScalarControl<ImGuiDataType_U32>(id, value);
	}

	bool DrawUlongControl(const char* id, uint64_t& value)
	{
		return DrawScalarControl<ImGuiDataType_U64>(id, value);
	}

	bool DrawByteControl(const char* id, uint8_t& value)
	{
		return DrawScalarControl<ImGuiDataType_U8>(id, value);
	}

	bool DrawSbyteControl(const char* id, int8_t& value)
	{
		return DrawScalarControl<ImGuiDataType_S8>(id, value);
	}

	bool DrawCharControl(const char* id, char& value)
	{
		return DrawScalarControl<ImGuiDataType_S8>(id, value);
	}

	bool DrawShortControl(const char* id, short& value)
	{
		return DrawScalarControl<ImGuiDataType_S16>(id, value);
	}

	bool DrawUshortControl(const char* id, uint16_t& value)
	{
		return DrawScalarControl<ImGuiDataType_U16>(id, value);
	}

	bool DrawDoubleControl(const char* id, double& value)
	{
		return ImGui::InputDouble(id, &value);
	}

	bool DrawScriptVec2Control(const char* id, glm::vec2& value)
	{
		return UI::DrawFieldVec2Control(id, value, 0.0f, ImGui::GetColumnWidth());
	}

	bool DrawScriptVec3Control(const char* id, glm::vec3& value)
	{
		return UI::DrawFieldVec3Control(id, value, 0.0f, ImGui::GetColumnWidth());
	}

	bool DrawScriptVec4Control(const char* id, glm::vec4& value)
	{
		return ImGui::ColorEdit4(id, glm::value_ptr(value));
	}

	template <typename Code, typename ToStringFn>
	bool DrawCodeCombo(const char* id, Code& value, Code first, Code last, ToStringFn toString)
	{
		bool changed = false;

		if (ImGui::BeginCombo(id, toString(value)))
		{
			for (Code candidate = first; candidate <= last; ++candidate)
			{
				const bool isSelected = value == candidate;
				std::string_view label = toString(candidate);
				if (label == "Unknown")
					continue;

				if (ImGui::Selectable(label.data(), isSelected))
				{
					value = candidate;
					changed = true;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return changed;
	}

	bool DrawKeyCombo(const char* id, KeyCode& value)
	{
		return DrawCodeCombo(id, value, static_cast<KeyCode>(Key::Space), static_cast<KeyCode>(Key::Menu), Key::ToString);
	}

	bool DrawMouseCombo(const char* id, MouseCode& value)
	{
		return DrawCodeCombo(id, value, static_cast<MouseCode>(Mouse::Button0), static_cast<MouseCode>(Mouse::ButtonLast), Mouse::ToString);
	}

	std::string EntityReferenceLabel(Entity context, UUID entityId)
	{
		if (entityId == 0)
			return "None";

		Scene* sceneContext = context.GetScene();
		if (!sceneContext)
			return "Missing Entity";

		Entity referencedEntity = sceneContext->FindEntityByUUID(entityId);
		if (!referencedEntity)
			return "Missing Entity";

		return referencedEntity.GetName();
	}

	bool DrawStringControl(const char* id, std::string& value)
	{
		return ImGui::InputText(id, &value);
	}

	bool DrawEntityControl(const char* id, UUID& value, Entity context)
	{
		bool changed = false;
		const std::string label = EntityReferenceLabel(context, value);

		ImGui::PushID(id);

		const float clearButtonWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float width = ImGui::GetContentRegionAvail().x;
		const float pickerWidth = value == 0 ? width : std::max(0.0f, width - clearButtonWidth - spacing);

		ImGui::Button(label.c_str(), ImVec2(pickerWidth, 0.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneEntityPayloadType))
			{
				WHP_CORE_ASSERT(payload->DataSize == sizeof(UUID), "Invalid entity drag payload size!");
				value = *static_cast<const UUID*>(payload->Data);
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}

		if (value != 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(clearButtonWidth, 0.0f)))
			{
				value = UUID(0);
				changed = true;
			}
		}

		ImGui::PopID();
		return changed;
	}

	struct ScenePickerItem
	{
		AssetHandle m_Handle = 0;
		std::filesystem::path m_Path;
		std::string m_Label;
	};

	std::vector<ScenePickerItem> CollectScenePickerItems()
	{
		std::vector<ScenePickerItem> items;
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return items;

		activeProject->GetEditorAssetManager()->GetAssetRegistry().Foreach(AssetType::Scene, [&](const AssetRegistry::ValueType& value)
			{
				ScenePickerItem item;
				item.m_Handle = value.first;
				item.m_Path = value.second.m_Filepath;
				item.m_Label = value.second.m_Filepath.stem().string();
				if (item.m_Label.empty())
					item.m_Label = value.second.m_Filepath.filename().string();
				items.push_back(std::move(item));
			});

		std::sort(items.begin(), items.end(), [](const ScenePickerItem& lhs, const ScenePickerItem& rhs)
			{
				return lhs.m_Path.generic_string() < rhs.m_Path.generic_string();
			});

		return items;
	}

	std::string SceneReferenceLabel(AssetHandle handle)
	{
		if (handle == 0)
			return "None";

		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager() ||
			!activeProject->GetEditorAssetManager()->IsAssetHandleValid(handle) ||
			activeProject->GetEditorAssetManager()->GetAssetType(handle) != AssetType::Scene)
			return "Missing Scene";

		const std::filesystem::path& path = activeProject->GetEditorAssetManager()->GetFilepath(handle);
		std::string label = path.stem().string();
		if (label.empty())
			label = path.filename().string();
		return label;
	}

	std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	bool SceneMatchesQuery(const ScenePickerItem& scene, const std::string& query)
	{
		if (query.empty())
			return true;

		const std::string loweredQuery = ToLower(query);
		return ToLower(scene.m_Label).find(loweredQuery) != std::string::npos ||
			ToLower(scene.m_Path.generic_string()).find(loweredQuery) != std::string::npos;
	}

	bool DrawSceneControl(const char* id, uint64_t& value)
	{
		static std::string s_SceneSearchQuery;
		bool changed = false;
		AssetHandle currentHandle(value);
		std::string label = SceneReferenceLabel(currentHandle);

		ImGui::PushID(id);

		const float clearButtonWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float width = ImGui::GetContentRegionAvail().x;
		const float pickerWidth = currentHandle == 0 ? width : std::max(0.0f, width - clearButtonWidth - spacing);

		if (ImGui::Button(label.c_str(), ImVec2(pickerWidth, 0.0f)))
			ImGui::OpenPopup("ScenePickerPopup");

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const AssetHandle droppedHandle = UI::ReadAssetReferencePayload(payload).m_Handle;
				Ref<Project> activeProject = Project::GetActive();
				if (activeProject && activeProject->GetEditorAssetManager() &&
					activeProject->GetEditorAssetManager()->IsAssetHandleValid(droppedHandle) &&
					activeProject->GetEditorAssetManager()->GetAssetType(droppedHandle) == AssetType::Scene)
				{
					value = droppedHandle;
					changed = true;
				}
				else
				{
					WHP_CORE_WARN("[Asset Manager] Wrong Asset type!");
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (currentHandle != 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(clearButtonWidth, 0.0f)))
			{
				value = 0;
				changed = true;
			}
		}

		if (ImGui::BeginPopup("ScenePickerPopup"))
		{
			ImGui::SetNextItemWidth(260.0f);
			ImGui::InputTextWithHint("##SceneSearch", "Search scenes", &s_SceneSearchQuery);
			ImGui::Separator();

			if (ImGui::Selectable("None", currentHandle == 0))
			{
				value = 0;
				changed = true;
			}

			const std::vector<ScenePickerItem> scenes = CollectScenePickerItems();
			if (!scenes.empty())
				ImGui::Separator();

			size_t visibleCount = 0;
			for (const ScenePickerItem& scene : scenes)
			{
				if (!SceneMatchesQuery(scene, s_SceneSearchQuery))
					continue;

				++visibleCount;
				ImGui::PushID(static_cast<int>((uint64_t)scene.m_Handle & 0xffffffffu));
				const bool selected = scene.m_Handle == currentHandle;
				if (ImGui::Selectable(scene.m_Label.c_str(), selected))
				{
					value = scene.m_Handle;
					changed = true;
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", scene.m_Path.generic_string().c_str());

				if (selected)
					ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}

			if (visibleCount == 0 && !s_SceneSearchQuery.empty())
				ImGui::TextDisabled("No matching scenes");

			ImGui::EndPopup();
		}

		ImGui::PopID();
		return changed;
	}

	template <UI::ScriptFieldDraw DrawMode, typename T, typename DrawFn>
	void DrawValueContents(const ScriptField& field, Entity entity, const std::string& className, const std::string& id, DrawFn draw)
	{
		if constexpr (DrawMode == UI::ScriptFieldDraw::WhileSceneRunning)
		{
			(void)className;
			Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
			if (!scriptInstance)
				return;

			T data = scriptInstance->GetFieldValue<T>(field.m_Name);
			if (draw(id.c_str(), data))
				scriptInstance->SetFieldValue<T>(field.m_Name, data);
		}
		else if constexpr (DrawMode == UI::ScriptFieldDraw::SetInTheEditor)
		{
			(void)className;
			ScriptFieldInstance& scriptField = EditorField(entity, field);
			T data = scriptField.GetValue<T>();
			if (draw(id.c_str(), data))
				scriptField.SetValue<T>(data);
		}
		else
		{
			ScriptFieldInstance& scriptField = BaseField(className, field);
			T data = scriptField.GetValue<T>();
			if (draw(id.c_str(), data))
				OverrideField(entity, field).SetValue<T>(data);
		}
	}

	template <UI::ScriptFieldDraw DrawMode, typename T, typename DrawFn>
	void DrawValue(const ScriptField& field, Entity entity, const std::string& className, bool inTable, DrawFn draw)
	{
		TableRowScope row(inTable, field.m_Name.c_str());
		DrawValueContents<DrawMode, T>(field, entity, className, ControlLabel(field, inTable), draw);
	}

	void DrawUnsupportedArray(const ScriptField& field, bool inTable)
	{
		TableRowScope row(inTable, field.m_Name.c_str());
		ImGui::TextDisabled("Unsupported array");
	}

	template <UI::ScriptFieldDraw DrawMode>
	void DrawStringField(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		if (field.m_IsArray)
		{
			DrawUnsupportedArray(field, inTable);
			return;
		}

		TableRowScope row(inTable, field.m_Name.c_str());
		const std::string id = ControlLabel(field, inTable);

		if constexpr (DrawMode == UI::ScriptFieldDraw::WhileSceneRunning)
		{
			(void)className;
			Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
			if (!scriptInstance)
				return;

			std::string data = scriptInstance->GetFieldString(field.m_Name);
			if (DrawStringControl(id.c_str(), data))
				scriptInstance->SetFieldString(field.m_Name, data);
		}
		else if constexpr (DrawMode == UI::ScriptFieldDraw::SetInTheEditor)
		{
			(void)className;
			ScriptFieldInstance& scriptField = EditorField(entity, field);
			std::string data = scriptField.GetStringValue();
			if (DrawStringControl(id.c_str(), data))
				scriptField.SetStringValue(data);
		}
		else
		{
			ScriptFieldInstance& scriptField = BaseField(className, field);
			std::string data = scriptField.GetStringValue();
			if (DrawStringControl(id.c_str(), data))
				OverrideField(entity, field).SetStringValue(data);
		}
	}

	template <UI::ScriptFieldDraw DrawMode>
	void DrawEntityField(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		if (field.m_IsArray)
		{
			DrawUnsupportedArray(field, inTable);
			return;
		}

		TableRowScope row(inTable, field.m_Name.c_str());
		const std::string id = ControlLabel(field, inTable);

		if constexpr (DrawMode == UI::ScriptFieldDraw::WhileSceneRunning)
		{
			(void)className;
			Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
			if (!scriptInstance)
				return;

			UUID data = scriptInstance->GetFieldEntity(field.m_Name);
			if (DrawEntityControl(id.c_str(), data, entity))
				scriptInstance->SetFieldEntity(field.m_Name, data);
		}
		else if constexpr (DrawMode == UI::ScriptFieldDraw::SetInTheEditor)
		{
			(void)className;
			ScriptFieldInstance& scriptField = EditorField(entity, field);
			UUID data = scriptField.GetEntityValue();
			if (DrawEntityControl(id.c_str(), data, entity))
				scriptField.SetEntityValue(data);
		}
		else
		{
			ScriptFieldInstance& scriptField = BaseField(className, field);
			UUID data = scriptField.GetEntityValue();
			if (DrawEntityControl(id.c_str(), data, entity))
				OverrideField(entity, field).SetEntityValue(data);
		}
	}

	template <UI::ScriptFieldDraw DrawMode>
	void DrawSceneField(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		if (field.m_IsArray)
		{
			DrawUnsupportedArray(field, inTable);
			return;
		}

		TableRowScope row(inTable, field.m_Name.c_str());
		const std::string id = ControlLabel(field, inTable);

		if constexpr (DrawMode == UI::ScriptFieldDraw::WhileSceneRunning)
		{
			(void)className;
			Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
			if (!scriptInstance)
				return;

			uint64_t data = scriptInstance->GetFieldValue<uint64_t>(field.m_Name);
			if (DrawSceneControl(id.c_str(), data))
				scriptInstance->SetFieldValue<uint64_t>(field.m_Name, data);
		}
		else if constexpr (DrawMode == UI::ScriptFieldDraw::SetInTheEditor)
		{
			(void)className;
			ScriptFieldInstance& scriptField = EditorField(entity, field);
			uint64_t data = scriptField.GetValue<uint64_t>();
			if (DrawSceneControl(id.c_str(), data))
				scriptField.SetValue<uint64_t>(data);
		}
		else
		{
			ScriptFieldInstance& scriptField = BaseField(className, field);
			uint64_t data = scriptField.GetValue<uint64_t>();
			if (DrawSceneControl(id.c_str(), data))
				OverrideField(entity, field).SetValue<uint64_t>(data);
		}
	}

	template <typename OnResize>
	bool DrawArraySizeControl(const ScriptField& field, size_t size, bool allowResize, OnResize onResize)
	{
		bool resized = false;
		int sizeValue = size > static_cast<size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : static_cast<int>(size);

		ImGui::PushID(field.m_Name.c_str());
		ImGui::SetNextItemWidth(96.0f);

		if (!allowResize)
			ImGui::BeginDisabled();

		if (ImGui::InputInt("Size", &sizeValue))
		{
			const size_t requestedSize = SanitizeArraySize(sizeValue);
			if (requestedSize != size)
			{
				onResize(requestedSize);
				resized = true;
			}
		}

		if (!allowResize)
			ImGui::EndDisabled();

		if (allowResize)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("+"))
			{
				onResize(size + 1);
				resized = true;
			}

			ImGui::SameLine();
			if (size == 0)
				ImGui::BeginDisabled();

			if (ImGui::SmallButton("Clear"))
			{
				onResize(0);
				resized = true;
			}

			if (size == 0)
				ImGui::EndDisabled();
		}

		ImGui::PopID();
		return resized;
	}

	template <typename T, typename DrawFn, typename OnChange, typename OnRemove>
	bool DrawArrayTable(const ScriptField& field, T* values, size_t size, bool allowRemove, DrawFn draw, OnChange onChange, OnRemove onRemove)
	{
		if (size == 0)
		{
			ImGui::TextDisabled("Empty");
			return false;
		}

		if (!values && size > 0)
		{
			ImGui::TextDisabled("Unavailable");
			return false;
		}

		const std::string tableId = ArrayTableId(field);
		const int columnCount = allowRemove ? 3 : 2;
		if (!ImGui::BeginTable(tableId.c_str(), columnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			return false;

		ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 48.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		if (allowRemove)
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 32.0f);

		size_t removeIndex = static_cast<size_t>(-1);

		for (size_t i = 0; i < size; ++i)
		{
			const std::string rowLabel = ArrayRowLabel(i);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(rowLabel.c_str());

			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-1.0f);
			const std::string id = ArrayControlLabel(field, rowLabel);
			if (draw(id.c_str(), values[i]))
				onChange(i);
			ImGui::PopItemWidth();

			if (allowRemove)
			{
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::SmallButton("X"))
					removeIndex = i;
				ImGui::PopID();
			}
		}

		ImGui::EndTable();

		if (removeIndex != static_cast<size_t>(-1))
		{
			onRemove(removeIndex);
			return true;
		}

		return false;
	}

	template <typename T>
	std::unique_ptr<T[]> CopyArrayValues(T* source, size_t size)
	{
		if (size == 0)
			return nullptr;

		auto values = std::make_unique<T[]>(size);
		if (source)
		{
			for (size_t i = 0; i < size; ++i)
				values[i] = source[i];
		}
		return values;
	}

	template <typename T>
	std::unique_ptr<T[]> ResizeArrayValues(T* source, size_t oldSize, size_t newSize)
	{
		if (newSize == 0)
			return nullptr;

		auto values = std::make_unique<T[]>(newSize);
		const size_t copySize = oldSize < newSize ? oldSize : newSize;
		if (source)
		{
			for (size_t i = 0; i < copySize; ++i)
				values[i] = source[i];
		}
		return values;
	}

	template <typename T>
	std::unique_ptr<T[]> RemoveArrayValue(T* source, size_t size, size_t removeIndex)
	{
		if (size <= 1)
			return nullptr;

		auto values = std::make_unique<T[]>(size - 1);
		size_t targetIndex = 0;
		for (size_t i = 0; i < size; ++i)
		{
			if (i == removeIndex)
				continue;

			values[targetIndex++] = source ? source[i] : T{};
		}
		return values;
	}

	template <typename T, typename DrawFn, typename OnChange, typename OnResize, typename OnRemove>
	void DrawArrayEditor(const ScriptField& field, T* values, size_t size, bool allowResize, DrawFn draw, OnChange onChange, OnResize onResize, OnRemove onRemove)
	{
		if (DrawArraySizeControl(field, size, allowResize, onResize))
			return;

		DrawArrayTable(field, values, size, allowResize, draw, onChange, onRemove);
	}

	template <UI::ScriptFieldDraw DrawMode, typename T, typename DrawFn>
	void DrawArrayContents(const ScriptField& field, Entity entity, const std::string& className, DrawFn draw)
	{
		if constexpr (DrawMode == UI::ScriptFieldDraw::WhileSceneRunning)
		{
			(void)className;
			Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
			if (!scriptInstance)
				return;

			size_t size = 0;
			T* rawArray = scriptInstance->GetFieldArray<T>(field.m_Name, &size);
			DrawArrayEditor(field, rawArray, size, false, draw, [&](size_t index)
				{
					scriptInstance->SetFieldArrayIndex(field.m_Name, index, rawArray[index]);
				}, [&](size_t)
				{
				}, [&](size_t)
				{
				});
		}
		else if constexpr (DrawMode == UI::ScriptFieldDraw::SetInTheEditor)
		{
			(void)className;
			ScriptFieldInstance& scriptField = EditorField(entity, field);
			const size_t size = scriptField.GetArraySize<T>();
			T* rawArray = scriptField.GetValueArray<T>();

			DrawArrayEditor(field, rawArray, size, true, draw, [&](size_t)
				{
					scriptField.SetValueArray<T>(rawArray, size);
				}, [&](size_t newSize)
				{
					auto values = ResizeArrayValues<T>(rawArray, size, newSize);
					scriptField.SetValueArray<T>(values.get(), newSize);
				}, [&](size_t removeIndex)
				{
					const size_t newSize = size > 0 ? size - 1 : 0;
					auto values = RemoveArrayValue<T>(rawArray, size, removeIndex);
					scriptField.SetValueArray<T>(values.get(), newSize);
				});
		}
		else
		{
			ScriptFieldInstance& scriptField = BaseField(className, field);
			const size_t size = scriptField.GetArraySize<T>();
			T* rawArray = scriptField.GetValueArray<T>();
			auto values = CopyArrayValues<T>(rawArray, size);

			DrawArrayEditor(field, values.get(), size, true, draw, [&](size_t)
				{
					OverrideField(entity, field).SetValueArray<T>(values.get(), size);
				}, [&](size_t newSize)
				{
					auto resizedValues = ResizeArrayValues<T>(rawArray, size, newSize);
					OverrideField(entity, field).SetValueArray<T>(resizedValues.get(), newSize);
				}, [&](size_t removeIndex)
				{
					const size_t newSize = size > 0 ? size - 1 : 0;
					auto resizedValues = RemoveArrayValue<T>(rawArray, size, removeIndex);
					OverrideField(entity, field).SetValueArray<T>(resizedValues.get(), newSize);
				});
		}
	}

	template <UI::ScriptFieldDraw DrawMode, typename T, typename DrawFn>
	void DrawScriptField(const ScriptField& field, Entity entity, const std::string& className, bool inTable, DrawFn draw)
	{
		if (!field.m_IsArray)
		{
			DrawValue<DrawMode, T>(field, entity, className, inTable, draw);
			return;
		}

		TableRowScope row(inTable, field.m_Name.c_str());
		DrawArrayContents<DrawMode, T>(field, entity, className, draw);
	}
}

namespace UI
{
#define WHIP_DEFINE_SCRIPT_FIELD(SCRIPT_TYPE, VALUE_TYPE, DRAW_FUNC) \
	template <> \
	void DrawField<ScriptFieldType::SCRIPT_TYPE, ScriptFieldDraw::WhileSceneRunning>(const ScriptField& field, Entity entity, const std::string& className, bool inTable) \
	{ \
		DrawScriptField<ScriptFieldDraw::WhileSceneRunning, VALUE_TYPE>(field, entity, className, inTable, DRAW_FUNC); \
	} \
	template <> \
	void DrawField<ScriptFieldType::SCRIPT_TYPE, ScriptFieldDraw::SetInTheEditor>(const ScriptField& field, Entity entity, const std::string& className, bool inTable) \
	{ \
		DrawScriptField<ScriptFieldDraw::SetInTheEditor, VALUE_TYPE>(field, entity, className, inTable, DRAW_FUNC); \
	} \
	template <> \
	void DrawField<ScriptFieldType::SCRIPT_TYPE, ScriptFieldDraw::WithBaseValue>(const ScriptField& field, Entity entity, const std::string& className, bool inTable) \
	{ \
		DrawScriptField<ScriptFieldDraw::WithBaseValue, VALUE_TYPE>(field, entity, className, inTable, DRAW_FUNC); \
	}

	WHIP_DEFINE_SCRIPT_FIELD(Float, float, DrawFloatControl)
	WHIP_DEFINE_SCRIPT_FIELD(Int, int, DrawIntControl)
	WHIP_DEFINE_SCRIPT_FIELD(Bool, bool, DrawBoolControl)
	WHIP_DEFINE_SCRIPT_FIELD(Long, int64_t, DrawLongControl)
	WHIP_DEFINE_SCRIPT_FIELD(Vector2, glm::vec2, DrawScriptVec2Control)
	WHIP_DEFINE_SCRIPT_FIELD(Vector3, glm::vec3, DrawScriptVec3Control)
	WHIP_DEFINE_SCRIPT_FIELD(Vector4, glm::vec4, DrawScriptVec4Control)
	WHIP_DEFINE_SCRIPT_FIELD(UInt, uint32_t, DrawUintControl)
	WHIP_DEFINE_SCRIPT_FIELD(ULong, uint64_t, DrawUlongControl)
	WHIP_DEFINE_SCRIPT_FIELD(Double, double, DrawDoubleControl)
	WHIP_DEFINE_SCRIPT_FIELD(Byte, uint8_t, DrawByteControl)
	WHIP_DEFINE_SCRIPT_FIELD(SByte, int8_t, DrawSbyteControl)
	WHIP_DEFINE_SCRIPT_FIELD(Char, char, DrawCharControl)
	WHIP_DEFINE_SCRIPT_FIELD(Short, short, DrawShortControl)
	WHIP_DEFINE_SCRIPT_FIELD(UShort, uint16_t, DrawUshortControl)
	WHIP_DEFINE_SCRIPT_FIELD(KeyCode, KeyCode, DrawKeyCombo)
	WHIP_DEFINE_SCRIPT_FIELD(MouseCode, MouseCode, DrawMouseCombo)

#undef WHIP_DEFINE_SCRIPT_FIELD

	template <>
	void DrawField<ScriptFieldType::String, ScriptFieldDraw::WhileSceneRunning>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawStringField<ScriptFieldDraw::WhileSceneRunning>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::String, ScriptFieldDraw::SetInTheEditor>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawStringField<ScriptFieldDraw::SetInTheEditor>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::String, ScriptFieldDraw::WithBaseValue>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawStringField<ScriptFieldDraw::WithBaseValue>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Entity, ScriptFieldDraw::WhileSceneRunning>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawEntityField<ScriptFieldDraw::WhileSceneRunning>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Entity, ScriptFieldDraw::SetInTheEditor>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawEntityField<ScriptFieldDraw::SetInTheEditor>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Entity, ScriptFieldDraw::WithBaseValue>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawEntityField<ScriptFieldDraw::WithBaseValue>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Scene, ScriptFieldDraw::WhileSceneRunning>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawSceneField<ScriptFieldDraw::WhileSceneRunning>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Scene, ScriptFieldDraw::SetInTheEditor>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawSceneField<ScriptFieldDraw::SetInTheEditor>(field, entity, className, inTable);
	}

	template <>
	void DrawField<ScriptFieldType::Scene, ScriptFieldDraw::WithBaseValue>(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		DrawSceneField<ScriptFieldDraw::WithBaseValue>(field, entity, className, inTable);
	}

	template <ScriptFieldDraw DrawMode>
	void DrawFieldByType(const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		switch (field.m_Type)
		{
		case ScriptFieldType::Float: DrawField<ScriptFieldType::Float, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Int: DrawField<ScriptFieldType::Int, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Bool: DrawField<ScriptFieldType::Bool, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Long: DrawField<ScriptFieldType::Long, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Vector3: DrawField<ScriptFieldType::Vector3, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Vector2: DrawField<ScriptFieldType::Vector2, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Vector4: DrawField<ScriptFieldType::Vector4, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::UInt: DrawField<ScriptFieldType::UInt, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::ULong: DrawField<ScriptFieldType::ULong, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Double: DrawField<ScriptFieldType::Double, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Byte: DrawField<ScriptFieldType::Byte, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::SByte: DrawField<ScriptFieldType::SByte, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Char: DrawField<ScriptFieldType::Char, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Short: DrawField<ScriptFieldType::Short, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::UShort: DrawField<ScriptFieldType::UShort, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::KeyCode: DrawField<ScriptFieldType::KeyCode, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::MouseCode: DrawField<ScriptFieldType::MouseCode, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::String: DrawField<ScriptFieldType::String, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Entity: DrawField<ScriptFieldType::Entity, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::Scene: DrawField<ScriptFieldType::Scene, DrawMode>(field, entity, className, inTable); break;
		case ScriptFieldType::None:
		case ScriptFieldType::Logger:
		default:
			break;
		}
	}

	void DrawFieldByType(ScriptFieldDraw drawMode, const ScriptField& field, Entity entity, const std::string& className, bool inTable)
	{
		switch (drawMode)
		{
		case ScriptFieldDraw::WhileSceneRunning:
			DrawFieldByType<ScriptFieldDraw::WhileSceneRunning>(field, entity, className, inTable);
			break;
		case ScriptFieldDraw::SetInTheEditor:
			DrawFieldByType<ScriptFieldDraw::SetInTheEditor>(field, entity, className, inTable);
			break;
		case ScriptFieldDraw::WithBaseValue:
			DrawFieldByType<ScriptFieldDraw::WithBaseValue>(field, entity, className, inTable);
			break;
		}
	}
}

_WHIP_END
