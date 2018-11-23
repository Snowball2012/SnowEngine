#pragma once

#include "Scene.h"

class UVScreenDensityCalculator
{
public:
	UVScreenDensityCalculator( Scene* scene );

	void Update( CameraID camera, const D3D12_VIEWPORT& viewport );

	void CalcUVDensityInObjectSpace( StaticSubmesh& submesh );

private:
	Scene* m_scene;
};