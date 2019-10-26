#pragma once

#include "Components.h"

#include "EntityContainer.h"

using World = EntityContainer<
    Transform,
    DrawableMesh
>;