#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

struct Vertex
{
	glm::vec2 pos;
	alignas(16) glm::vec3 color;
	glm::vec2 texcoord;

	static std::array<RHIPrimitiveAttributeInfo, 3> GetRHIAttributes();
};

struct Matrices
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};