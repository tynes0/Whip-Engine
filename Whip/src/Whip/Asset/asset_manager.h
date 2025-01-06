#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Project/project.h>

#include "asset_manager_base.h"

_WHIP_START

class asset_manager
{
public:
	template <class T, std::enable_if_t<is_asset_v<T>, int> = 0>
	static ref<T> get_asset(asset_handle handle)
	{
		ref<asset> l_asset = project::get_active()->get_asset_manager()->get_asset(handle);
		return std::static_pointer_cast<T>(l_asset);
	}

	static bool is_asset_handle_valid(asset_handle handle)
	{
		return project::get_active()->get_asset_manager()->is_asset_handle_valid(handle);
	}

	static bool is_asset_loaded(asset_handle handle)
	{
		return project::get_active()->get_asset_manager()->is_asset_loaded(handle);
	}

	static asset_type get_asset_type(asset_handle handle)
	{
		return project::get_active()->get_asset_manager()->get_asset_type(handle);
	}

	static const asset_metadata& get_asset_metadata(asset_handle handle)
	{
		return project::get_active()->get_asset_manager()->get_metadata(handle);
	}

	static void add_registry(asset_handle handle, const asset_metadata& metadata)
	{
		project::get_active()->get_asset_manager()->add_registry(handle, metadata);
	}
};

_WHIP_END
