#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include "asset_metadata.h"
#include "asset.h"

#include <unordered_map>

_WHIP_START

using asset_map = std::unordered_map<asset_handle, ref<asset>>;

class asset_manager_base
{
public:
	virtual ref<asset> get_asset(asset_handle handle) = 0;

	virtual bool is_asset_handle_valid(asset_handle handle) const = 0;
	virtual bool is_asset_loaded(asset_handle handle) const = 0;
	virtual asset_type get_asset_type(asset_handle handle) const = 0;
	virtual const asset_metadata& get_metadata(asset_handle handle) const = 0;
	virtual void add_registry(asset_handle handle, const asset_metadata& metadata) = 0;
};

_WHIP_END
