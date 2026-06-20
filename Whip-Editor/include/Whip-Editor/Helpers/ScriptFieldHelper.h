#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scripting/ScriptEngine.h>

_WHIP_START

namespace UI
{
	enum class ScriptFieldDraw : uint8_t
	{
		WhileSceneRunning,
		SetInTheEditor,
		WithBaseValue
	};

	template <ScriptFieldType Sft, ScriptFieldDraw Sfdt>
	void DrawField(const ScriptField& field, Entity entity, const std::string& className, bool inTable = false) = delete;

	void DrawFieldByType(ScriptFieldDraw drawMode, const ScriptField& field, Entity entity, const std::string& className, bool inTable = false);
}

_WHIP_END
