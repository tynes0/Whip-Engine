#include <Whip-Assistant/EngineKnowledgeManifest.h>

#include <Whip-Assistant/AssistantToolRegistry.h>

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

		void AppendMembers(std::ostringstream& stream, const char* label, const std::vector<KnowledgeMember>& members)
		{
			if (members.empty())
				return;

			stream << "  " << label << ":\n";
			for (const KnowledgeMember& member : members)
			{
				stream << "    - " << member.m_Name;
				if (!member.m_Type.empty())
					stream << " : " << member.m_Type;
				if (!member.m_Description.empty())
					stream << " - " << member.m_Description;
				stream << '\n';
			}
		}

		const EngineKnowledgeManifest Manifest =
		{
			.m_Name = "Whip Engine AI Knowledge Manifest",
			.m_Version = "0.1",
			.m_ScriptCallbacks =
			{
				{ "OnCreate()", "Called once when the script instance is created." },
				{ "OnUpdate(float ts)", "Called every frame; use ts as delta time." },
				{ "OnDestroy()", "Called when the script instance is destroyed." },
				{ "OnColliderEnter(string tag)", "Called when a 2D collider enter event happens; receives the other collider tag." },
				{ "OnColliderExit(string tag)", "Called when a 2D collider exit event happens; receives the other collider tag." },
				{ "OnAnimationEvent(string eventName)", "Called from animation event tracks." }
			},
			.m_ScriptTypes =
			{
				{
					.m_Name = "Whip.Entity",
					.m_Category = "Base script type",
					.m_Description = "All user scripts derive from Entity, not MonoBehaviour.",
					.m_Properties =
					{
						{ "ID", "ulong", "Entity UUID." },
						{ "Translation", "Vector3", "World transform translation." },
						{ "Rotation", "Vector3", "World transform rotation." },
						{ "Scale", "Vector3", "World transform scale." }
					},
					.m_Methods =
					{
						{ "HasComponent<T>()", "bool", "Checks whether the entity has component T." },
						{ "GetComponent<T>()", "T", "Returns component T or null when missing." },
						{ "FindEntityByName(string name)", "Entity", "Finds an entity by name or returns null." },
						{ "As<T>()", "T", "Casts the current native script instance to a managed type." }
					}
				},
				{
					.m_Name = "TransformComponent",
					.m_Category = "Component",
					.m_Description = "Transform data for position, rotation, and scale.",
					.m_Properties =
					{
						{ "Translation", "Vector3", "Position." },
						{ "Rotation", "Vector3", "Euler rotation." },
						{ "Scale", "Vector3", "Scale." }
					}
				},
				{
					.m_Name = "Rigidbody2DComponent",
					.m_Category = "Component",
					.m_Description = "2D physics body wrapper.",
					.m_Properties =
					{
						{ "Type", "BodyType", "Static, Dynamic, or Kinematic." },
						{ "LinearVelocity", "Vector2", "Current linear velocity." },
						{ "AngularVelocity", "float", "Current angular velocity." },
						{ "GravityScale", "float", "Gravity multiplier." },
						{ "FixedRotation", "bool", "Locks body rotation." },
						{ "Enabled", "bool", "Whether the body is enabled." },
						{ "Awake", "bool", "Whether the body is awake." }
					},
					.m_Methods =
					{
						{ "ApplyForce(Vector2 force, bool wake)", "void", "Applies a force at the center of mass." },
						{ "ApplyLinearImpulse(Vector2 impulse, bool wake)", "void", "Applies a linear impulse." },
						{ "ApplyAngularImpulse(float impulse, bool wake)", "void", "Applies angular impulse." },
						{ "ApplyTorque(float torque, bool wake)", "void", "Applies torque." },
						{ "SetTransform(Vector2 position, float angle)", "void", "Moves the physics body." }
					}
				},
				{
					.m_Name = "AnimatorComponent",
					.m_Category = "Component",
					.m_Description = "Runtime animation controller access.",
					.m_Properties =
					{
						{ "Controller", "AssetHandle", "Assigned animation controller asset." },
						{ "Speed", "float", "Playback speed." },
						{ "IsPlaying", "bool", "Whether animation is playing." },
						{ "CurrentState", "string", "Current controller state name." }
					},
					.m_Methods =
					{
						{ "Play(string stateName = \"\")", "void", "Starts playback, optionally from a state." },
						{ "Stop()", "void", "Stops playback." },
						{ "SetBool/GetBool", "bool", "Reads or writes bool parameters." },
						{ "SetInt/GetInt", "int", "Reads or writes int parameters." },
						{ "SetFloat/GetFloat", "float", "Reads or writes float parameters." },
						{ "SetTrigger/ResetTrigger/IsTriggerSet", "trigger", "Trigger parameter helpers." }
					}
				},
				{
					.m_Name = "SpriteRendererComponent",
					.m_Category = "Component",
					.m_Description = "2D textured sprite renderer.",
					.m_Properties =
					{
						{ "TextureHandle", "AssetHandle", "Texture asset assigned to the sprite." },
						{ "Color", "Vector4", "Tint color." },
						{ "TilingFactor", "float", "Texture tiling multiplier." }
					}
				},
				{
					.m_Name = "CircleRendererComponent",
					.m_Category = "Component",
					.m_Description = "2D circle renderer.",
					.m_Properties =
					{
						{ "Color", "Vector4", "Tint color." },
						{ "Thickness", "float", "Circle thickness." },
						{ "Fade", "float", "Edge fade." }
					}
				},
				{
					.m_Name = "TextComponent",
					.m_Category = "Component",
					.m_Description = "Text renderer.",
					.m_Properties =
					{
						{ "Text", "string", "Displayed text." },
						{ "Color", "Vector4", "Text color." },
						{ "Kerning", "float", "Character spacing." },
						{ "LineSpacing", "float", "Line spacing." }
					}
				},
				{
					.m_Name = "CameraComponent",
					.m_Category = "Component",
					.m_Description = "Scene camera.",
					.m_Properties =
					{
						{ "Primary", "bool", "Whether this is the primary camera." },
						{ "FixedAspectRatio", "bool", "Locks aspect ratio." },
						{ "PerspectiveVerticalFOV", "float", "Perspective field of view." },
						{ "PerspectiveNearClip/PerspectiveFarClip", "float", "Perspective clipping planes." },
						{ "OrthographicSize", "float", "Orthographic camera size." },
						{ "OrthographicNearClip/OrthographicFarClip", "float", "Orthographic clipping planes." }
					}
				},
				{
					.m_Name = "BoxCollider2DComponent / CircleCollider2DComponent",
					.m_Category = "Component",
					.m_Description = "2D collider fixture data. Collision callbacks receive the collider Tag string.",
					.m_Properties =
					{
						{ "Tag", "string", "Tag delivered to OnColliderEnter/OnColliderExit." },
						{ "Size", "Vector2", "Collider size." },
						{ "Offset", "Vector2", "Collider local offset." },
						{ "Density", "float", "Physics density." },
						{ "Friction", "float", "Physics friction." },
						{ "Restitution", "float", "Bounciness." },
						{ "RestitutionThreshold", "float", "Bounce threshold." }
					}
				},
				{
					.m_Name = "Input",
					.m_Category = "Static API",
					.m_Description = "Keyboard and mouse input.",
					.m_Methods =
					{
						{ "IsKeyDown/IsKeyUp", "bool", "Current key state." },
						{ "IsKeyPressed/IsKeyReleased", "bool", "Edge-triggered key state." },
						{ "IsMouseButtonDown/IsMouseButtonUp", "bool", "Current mouse button state." },
						{ "IsMouseButtonPressed/IsMouseButtonReleased", "bool", "Edge-triggered mouse button state." },
						{ "GetMousePosition()", "Vector2", "Mouse position." }
					}
				},
				{
					.m_Name = "AudioComponent / AudioData",
					.m_Category = "Component",
					.m_Description = "Entity audio playback and audio clip data.",
					.m_Methods =
					{
						{ "CreateAudioData()", "AudioData", "Creates an audio slot." },
						{ "GetAudioData(int index)", "AudioData", "Gets audio slot by index." },
						{ "AddAudioData(AudioData audioData)", "void", "Adds an audio slot." },
						{ "RemoveAudioData(...)", "void", "Removes an audio slot." }
					}
				},
				{
					.m_Name = "AssetManager / SceneManager / Timer / Logger",
					.m_Category = "Static APIs",
					.m_Description = "Asset lookup, scene loading, timers, and logging utilities.",
					.m_Methods =
					{
						{ "AssetManager.ImportAsset(string path)", "AssetHandle", "Imports an asset path." },
						{ "AssetManager.DeleteAsset(ulong id)", "void", "Deletes an asset by handle id." },
						{ "AssetManager.IsAssetHandleValid/IsAssetLoaded", "bool", "Asset state checks." },
						{ "AssetManager.GetAssetType/GetFilepath", "AssetType/string", "Asset metadata helpers." },
						{ "SceneManager.LoadScene(...)", "bool", "Loads by AssetHandle, SceneReference, handle id, or scene name." },
						{ "SceneManager.FindScene(string sceneName)", "SceneReference", "Finds a scene by name." },
						{ "SceneManager.LoadStartScene/ReloadScene/UnloadScene", "bool", "Scene lifecycle helpers." },
						{ "Timer.SetTimeout/SetInterval/Pause/Resume/Stop/Clear/Exists", "timer", "Timer utilities." },
						{ "Logger.Log(...)", "void", "Writes to Whip logs." }
					}
				}
			},
			.m_ForbiddenApis =
			{
				"MonoBehaviour",
				"UnityEngine",
				"Collision",
				"OnCollisionEnter",
				"OnCollisionExit",
				"gameObject",
				"CompareTag",
				"Time.deltaTime"
			},
			.m_CodeEditRules =
			{
				"Use only APIs listed in this manifest or present in selected source/context.",
				"Null-check GetComponent<T>() results before use.",
				"Use OnColliderEnter(string tag) and OnColliderExit(string tag) for collision callbacks.",
				"Use OnUpdate(float ts) and the ts parameter for frame delta time.",
				"Do not claim a file changed until the editor applies the proposal.",
				"When changing a script, return one complete whip_script_edit block for the selected script path.",
				"Script edit block format: ```whip_script_edit, path: <Selected script path>, summary: <short summary>, ---BEGIN CONTENT---, complete C# file, ---END CONTENT---, ```."
			}
		};
	}

	const EngineKnowledgeManifest& GetEngineKnowledgeManifest()
	{
		return Manifest;
	}

	std::string BuildEngineKnowledgePrompt()
	{
		const EngineKnowledgeManifest& manifest = GetEngineKnowledgeManifest();
		std::ostringstream stream;
		stream << manifest.m_Name << " v" << manifest.m_Version << "\n";

		stream << "Script callbacks:\n";
		for (const KnowledgeCallback& callback : manifest.m_ScriptCallbacks)
			stream << "- " << callback.m_Signature << " - " << callback.m_Description << '\n';

		stream << "Script API surface:\n";
		for (const KnowledgeType& type : manifest.m_ScriptTypes)
		{
			stream << "- " << type.m_Name << " (" << type.m_Category << "): " << type.m_Description << '\n';
			AppendMembers(stream, "Properties", type.m_Properties);
			AppendMembers(stream, "Methods", type.m_Methods);
		}

		stream << BuildAssistantToolRegistryPrompt();

		stream << "Forbidden APIs:\n";
		for (const std::string& api : manifest.m_ForbiddenApis)
			stream << "- " << api << '\n';

		stream << "Code edit rules:\n";
		for (const std::string& rule : manifest.m_CodeEditRules)
			stream << "- " << rule << '\n';

		return stream.str();
	}

	std::string BuildEngineKnowledgeJson()
	{
		const EngineKnowledgeManifest& manifest = GetEngineKnowledgeManifest();
		std::ostringstream stream;
		stream << "{";
		stream << "\"name\":\"" << EscapeJson(manifest.m_Name) << "\",";
		stream << "\"version\":\"" << EscapeJson(manifest.m_Version) << "\",";
		stream << "\"scriptCallbacks\":[";
		for (size_t i = 0; i < manifest.m_ScriptCallbacks.size(); ++i)
		{
			if (i != 0)
				stream << ',';
			const KnowledgeCallback& callback = manifest.m_ScriptCallbacks[i];
			stream << "{\"signature\":\"" << EscapeJson(callback.m_Signature) << "\",\"description\":\"" << EscapeJson(callback.m_Description) << "\"}";
		}
		stream << "],\"assistantToolRegistry\":" << BuildAssistantToolRegistryJson() << ",\"forbiddenApis\":[";
		for (size_t i = 0; i < manifest.m_ForbiddenApis.size(); ++i)
		{
			if (i != 0)
				stream << ',';
			stream << '"' << EscapeJson(manifest.m_ForbiddenApis[i]) << '"';
		}
		stream << "]}";
		return stream.str();
	}
}

_WHIP_END
