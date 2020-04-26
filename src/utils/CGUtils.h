#pragma once

#include "MathUtils.h"

#include "utils/span.h"

// may be non-power-of-2
uint32_t CalcMipNumber( uint32_t resolution );

// containg_splits[i] == true means that the item must be rendered in split i
// multiple splits may be marked
// light_dir must be normalized
// assumes containing_splits initialized to {false}
bool FindPSSMFrustumsForItem(
	const DirectX::XMVECTOR& camera_to_item_center, float item_radius,
	const DirectX::XMVECTOR& light_dir, const span<const float>& split_distances,
	span<bool>& containing_splits);