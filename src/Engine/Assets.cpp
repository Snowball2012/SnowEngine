
#include "StdAfx.h"

#include "Assets.h"

#include "RHIUtils.h"

struct CubeVertex
{
    glm::vec3 position;
};

bool CubeAsset::Load( const JsonValue& data )
{
    m_status = AssetStatus::Ready;

    static std::array<CubeVertex, 8> vertices =
    {
        CubeVertex{ .position = { -0.5f, -0.5f, -0.5f } },
        CubeVertex{ .position = { -0.5f, -0.5f,  0.5f } },
        CubeVertex{ .position = { -0.5f,  0.5f, -0.5f } },
        CubeVertex{ .position = { -0.5f,  0.5f,  0.5f } },
        CubeVertex{ .position = {  0.5f, -0.5f, -0.5f } },
        CubeVertex{ .position = {  0.5f, -0.5f,  0.5f } },
        CubeVertex{ .position = {  0.5f,  0.5f, -0.5f } },
        CubeVertex{ .position = {  0.5f,  0.5f,  0.5f } },
    };
    RHI::BufferInfo vertex_buf_info = {};
    vertex_buf_info.size = sizeof( CubeVertex ) * vertices.size();
    vertex_buf_info.usage = RHIBufferUsageFlags::VertexBuffer | RHIBufferUsageFlags::AccelerationStructureInput;
    m_vertex_buffer = RHIUtils::CreateInitializedGPUBuffer( vertex_buf_info, vertices.data(), vertex_buf_info.size );

    static std::array<uint16_t, m_indices_count> indices =
    {
        0, 2, 1,
        2, 3, 1,

        4, 5, 6,
        6, 5, 7,

        0, 4, 6,
        0, 6, 2,

        1, 7, 5,
        1, 3, 7,

        0, 5, 4,
        0, 1, 5,

        2, 6, 3,
        3, 6, 7
    };
    RHI::BufferInfo index_buf_info = {};
    index_buf_info.size = sizeof( indices[0] ) * indices.size();
    index_buf_info.usage = RHIBufferUsageFlags::IndexBuffer | RHIBufferUsageFlags::AccelerationStructureInput;
    m_index_buffer = RHIUtils::CreateInitializedGPUBuffer( index_buf_info, indices.data(), index_buf_info.size );

    RHIASGeometryInfo blas_geom = {};
    blas_geom.type = RHIASGeometryType::Triangles;
    blas_geom.triangles.idx_buf = m_index_buffer.get();
    blas_geom.triangles.idx_type = GetIndexBufferType();
    blas_geom.triangles.vtx_buf = m_vertex_buffer.get();
    blas_geom.triangles.vtx_format = GetPositionBufferInfo().format;
    blas_geom.triangles.vtx_stride = RHIUtils::GetRHIFormatSize( blas_geom.triangles.vtx_format );
    m_blas = RHIUtils::CreateAS( blas_geom );

    return true;
}

RHIPrimitiveAttributeInfo CubeAsset::GetPositionBufferInfo() const
{
    RHIPrimitiveAttributeInfo attrib_info;
    attrib_info.semantic = "POSITION";
    attrib_info.format = RHIFormat::R32G32B32_SFLOAT;
    attrib_info.offset = offsetof( CubeVertex, position );
    return attrib_info;
}

size_t CubeAsset::GetPositionBufferStride() const
{
    return sizeof( CubeVertex );
}

const RHIIndexBufferType CubeAsset::GetIndexBufferType() const
{
    return RHIIndexBufferType::UInt16;
}
