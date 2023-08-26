
#include "StdAfx.h"

#include "Assets.h"

#include "RHIUtils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

// MeshAsset

struct MeshVertex
{
    glm::vec3 position;
};

RHIPrimitiveAttributeInfo MeshAsset::GetPositionBufferInfo() const
{
    RHIPrimitiveAttributeInfo attrib_info;
    attrib_info.semantic = "POSITION";
    attrib_info.format = RHIFormat::R32G32B32_SFLOAT;
    attrib_info.offset = offsetof( MeshVertex, position );
    return attrib_info;
}

size_t MeshAsset::GetPositionBufferStride() const
{
    return sizeof( MeshVertex );
}

const RHIIndexBufferType MeshAsset::GetIndexBufferType() const
{
    return m_indices_num > std::numeric_limits<uint16_t>::max() ? RHIIndexBufferType::UInt32 : RHIIndexBufferType::UInt16;
}

bool MeshAsset::Load( const JsonValue& data )
{
    JsonValue::ConstMemberIterator source = data.FindMember( "source" );
    if ( source == data.MemberEnd() || !source->value.IsString() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid mesh asset: \"source\" value is invalid", m_id.GetPath() );
        return false;
    }

    if ( !LoadFromObj( source->value.GetString() ) )
    {
        return false;
    }

    m_status = AssetStatus::Ready;
    return true;
}

bool MeshAsset::LoadFromObj( const char* path )
{
    // @todo: pre-reserve index array

    std::string inputfile = "cornell_box.obj";
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;

    std::string input_file_path = ToOSPath( path );

    bool tinyobjloader_retval = tinyobj::LoadObj( &attrib, &shapes, &materials, &err, input_file_path.c_str() );

    if ( !err.empty() )
    {
        SE_LOG_ERROR( Engine, "[TinyObjLoader]: %s", err.c_str() );
    }

    if ( tinyobjloader_retval == false )
    {
        SE_LOG_ERROR( Engine, "Could not load .obj file at <%s>", input_file_path.c_str() );
        return false;
    }

    std::vector<MeshVertex> vertices;
    std::vector<uint16_t> indices;

    // vertices array stores 3 elements per vertex
    vertices.reserve( attrib.vertices.size() / 3 );
    if ( attrib.vertices.size() % 3 != 0 )
    {
        SE_LOG_ERROR( Engine, "Obj file vertex array size not multiple of 3. File: <%s> ", input_file_path.c_str() );
        return false;
    }

    for ( size_t i = 0, num_vertices = attrib.vertices.size(); i < num_vertices; i += 3 )
    {
        tinyobj::real_t vx = attrib.vertices[i + 0];
        tinyobj::real_t vy = attrib.vertices[i + 1];
        tinyobj::real_t vz = attrib.vertices[i + 2];

        vertices.emplace_back( MeshVertex{ glm::vec3( vx, vy, vz ) } );
    }

    // Loop over shapes
    for ( size_t s = 0; s < shapes.size(); s++ ) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for ( size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++ ) {
            size_t fv = size_t( shapes[s].mesh.num_face_vertices[f] );

            if ( fv != 3 )
            {
                NOTIMPL;
            }

            // Loop over vertices in the face.
            for ( size_t v = 0; v < fv; v++ ) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                if ( idx.vertex_index > std::numeric_limits<uint16_t>::max() )
                {
                    NOTIMPL;
                }
                indices.emplace_back( idx.vertex_index );

                // Check if `normal_index` is zero or positive. negative = no normal data
                //if ( idx.normal_index >= 0 ) {
                //    tinyobj::real_t nx = attrib.normals[3 * size_t( idx.normal_index ) + 0];
                //    tinyobj::real_t ny = attrib.normals[3 * size_t( idx.normal_index ) + 1];
                //    tinyobj::real_t nz = attrib.normals[3 * size_t( idx.normal_index ) + 2];
                //}

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                //if ( idx.texcoord_index >= 0 ) {
                //    tinyobj::real_t tx = attrib.texcoords[2 * size_t( idx.texcoord_index ) + 0];
                //    tinyobj::real_t ty = attrib.texcoords[2 * size_t( idx.texcoord_index ) + 1];
                //}
                // Optional: vertex colors
                // tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
                // tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
                // tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
            }
            index_offset += fv;
        }
    }

    return LoadFromData( vertices, indices );
}

bool MeshAsset::LoadFromData( const std::span<MeshVertex>& vertices, const std::span<uint16_t>& indices )
{
    RHI::BufferInfo vertex_buf_info = {};
    vertex_buf_info.size = sizeof( MeshVertex ) * vertices.size();
    vertex_buf_info.usage = RHIBufferUsageFlags::VertexBuffer | RHIBufferUsageFlags::AccelerationStructureInput;
    m_vertex_buffer = RHIUtils::CreateInitializedGPUBuffer( vertex_buf_info, vertices.data(), vertex_buf_info.size );

    RHI::BufferInfo index_buf_info = {};
    index_buf_info.size = sizeof( indices[0] ) * indices.size();
    index_buf_info.usage = RHIBufferUsageFlags::IndexBuffer | RHIBufferUsageFlags::AccelerationStructureInput;
    m_index_buffer = RHIUtils::CreateInitializedGPUBuffer( index_buf_info, indices.data(), index_buf_info.size );
    m_indices_num = uint32_t( indices.size() );

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

// CubeAsset

bool CubeAsset::Load( const JsonValue& data )
{
    static std::array<MeshVertex, 8> vertices =
    {
        MeshVertex{.position = { -0.5f, -0.5f, -0.5f } },
        MeshVertex{.position = { -0.5f, -0.5f,  0.5f } },
        MeshVertex{.position = { -0.5f,  0.5f, -0.5f } },
        MeshVertex{.position = { -0.5f,  0.5f,  0.5f } },
        MeshVertex{.position = {  0.5f, -0.5f, -0.5f } },
        MeshVertex{.position = {  0.5f, -0.5f,  0.5f } },
        MeshVertex{.position = {  0.5f,  0.5f, -0.5f } },
        MeshVertex{.position = {  0.5f,  0.5f,  0.5f } },
    };

    static std::array<uint16_t, m_cube_indices_count> indices =
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

    if ( ! LoadFromData( vertices, indices ) )
    {
        return false;
    }

    m_status = AssetStatus::Ready;
    return true;
}