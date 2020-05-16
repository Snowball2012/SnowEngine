#pragma once

#include "Components.h"

#include <ecs/EntityContainer.h>

using World = EntityContainer<
    Transform,
    DrawableMesh,
	Camera
>;