#pragma once

#include "Scene.h"

class UVScreenDensityCalculator
{
public:
    UVScreenDensityCalculator( Scene* scene );

    void Update( const Camera& camera, const D3D12_VIEWPORT& viewport );

    void CalcUVDensityInObjectSpace( StaticSubmesh& submesh );

private:
    Scene* m_scene;
};