#pragma once

#include <DirectXMath.h>
#include "RenderData.h"

bool LoadObjFromFile( const std::string& filename, StaticMesh& mesh );
bool LoadFbxFromFile( const std::string& filename, StaticMesh& mesh );