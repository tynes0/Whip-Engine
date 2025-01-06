#pragma once

#include <Whip/Core/Core.h>

#include "asset_metadata.h"
#include "asset_manager_base.h"
#include "editor_asset_manager.h"

_WHIP_START

class runtime_asset_manager : public asset_manager_base
{
public:
	virtual ref<asset> get_asset(asset_handle handle) override;

	virtual bool is_asset_handle_valid(asset_handle handle) const override;
	virtual bool is_asset_loaded(asset_handle handle) const override;
	virtual asset_type get_asset_type(asset_handle handle) const override;
	virtual void add_registry(asset_handle handle, const asset_metadata& metadata) override;

	asset_handle import_asset(const std::filesystem::path& filepath);
	void delete_asset(asset_handle handle);

	const asset_metadata& get_metadata(asset_handle handle) const;
	const std::filesystem::path& get_filepath(asset_handle handle) const;
	const asset_registry& get_asset_registry() const { return m_asset_registry; }

	void runtime_stop();
	void set_editor_asset_manager(ref<editor_asset_manager> manager);
	void add_asset_copy(asset_handle handle, ref<asset> copy_of_asset);
private:
	bool is_runtime(asset_handle handle) const;

	asset_registry m_asset_registry;
	asset_map m_loaded_assets;
	ref<editor_asset_manager> m_editor_asset_manager;
};

_WHIP_END
