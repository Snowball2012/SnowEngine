#pragma once

#include "Scene.h"

class UVScreenDensityCalculator
{
public:
	UVScreenDensityCalculator( Scene* scene );

	void Update( CameraID camera );

	void CalcUVDensityInObjectSpace( StaticSubmesh& submesh );

private:
	Scene* m_scene;
};