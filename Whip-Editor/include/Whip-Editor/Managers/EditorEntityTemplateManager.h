#pragma once

#include <Whip.h>

#include "EditorManagerBase.h"

_WHIP_START

class EditorEntityTemplateManager : public EditorManagerBase // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	EditorEntityTemplateManager(EditorLayer* boundedLayer = nullptr);
	~EditorEntityTemplateManager() override;

	void SaveEntityTemplate(Entity entityIn);
	void ApplyEntityTemplate(Entity entityIn);
	void RevertEntityTemplate(Entity entityIn);
	void UnpackEntityTemplate(Entity entityIn);
	Entity FindPrefabRoot(Entity entityIn) const;
	void RemovePrefabLinksRecursive(Entity entityIn);
	bool InstantiateEntityTemplate(AssetHandle handle);
};

_WHIP_END
