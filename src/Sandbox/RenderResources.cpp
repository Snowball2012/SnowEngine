#include "StdAfx.h"

#include "RenderResources.h"

std::array<RHIPrimitiveAttributeInfo, 3> Vertex::GetRHIAttributes()
{
    std::array<RHIPrimitiveAttributeInfo, 3> attribute_descs = {};

    attribute_descs[0].semantic = "POSITION";
    attribute_descs[0].format = RHIFormat::R32G32_SFLOAT;
    attribute_descs[0].offset = offsetof(Vertex, pos);

    attribute_descs[1].semantic = "TEXCOORD0";
    attribute_descs[1].format = RHIFormat::R32G32B32_SFLOAT;
    attribute_descs[1].offset = offsetof(Vertex, color);

    attribute_descs[2].semantic = "TEXCOORD1";
    attribute_descs[2].format = RHIFormat::R32G32_SFLOAT;
    attribute_descs[2].offset = offsetof(Vertex, texcoord);

    return attribute_descs;
}