
#include "StdAfx.h"

#include "Assets.h"

#include "RHIUtils.h"
#include "Scene.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <stb/stb_image.h>

// MeshAsset

struct MeshVertex
{
    glm::vec3 position;
    glm::vec3 normal;
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

    JsonValue::ConstMemberIterator material_it = data.FindMember( "material" );
    if ( material_it != data.MemberEnd() )
    {
        if ( !material_it->value.IsString() )
        {
            SE_LOG_ERROR( Engine, "File %s parse error: \"material\" value is not a string", m_id.GetPath() );
        }
        else
        {
            m_default_material = LoadAsset<MaterialAsset>( material_it->value.GetString() );
        }
    }

    if ( m_default_material == nullptr )
    {
        SE_LOG_WARNING( Engine, "MeshAsset %s does not have a default material set", m_id.GetPath() );
    }

    m_status = AssetStatus::Ready;
    return true;
}

bool MeshAsset::LoadFromObj( const char* path )
{
    // @todo: pre-reserve index array
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

        vertices.emplace_back( MeshVertex{ glm::vec3( vx, vy, vz ), glm::vec3( 0, 0, 1 ) } );
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
    vertex_buf_info.usage = RHIBufferUsageFlags::VertexBuffer | RHIBufferUsageFlags::AccelerationStructureInput | RHIBufferUsageFlags::StructuredBuffer;
    m_vertex_buffer = RHIUtils::CreateInitializedGPUBuffer( vertex_buf_info, vertices.data(), vertex_buf_info.size );

    RHI::BufferInfo index_buf_info = {};
    index_buf_info.size = sizeof( indices[0] ) * indices.size();
    index_buf_info.usage = RHIBufferUsageFlags::IndexBuffer | RHIBufferUsageFlags::AccelerationStructureInput | RHIBufferUsageFlags::StructuredBuffer;
    m_index_buffer = RHIUtils::CreateInitializedGPUBuffer( index_buf_info, indices.data(), index_buf_info.size );
    m_indices_num = uint32_t( indices.size() );

    RHIASGeometryInfo blas_geom = {};
    blas_geom.type = RHIASGeometryType::Triangles;
    blas_geom.triangles.idx_buf = m_index_buffer.get();
    blas_geom.triangles.idx_type = GetIndexBufferType();
    blas_geom.triangles.vtx_buf = m_vertex_buffer.get();
    blas_geom.triangles.vtx_format = GetPositionBufferInfo().format;
    blas_geom.triangles.vtx_stride = GetPositionBufferStride();
    blas_geom.triangles.vtx_offset = GetPositionBufferInfo().offset;
    m_blas = RHIUtils::CreateAS( blas_geom );

    m_global_geom_index = GetRenderer().GetGlobalDescriptors().AddGeometry( RHIBufferViewInfo{ m_vertex_buffer.get() }, RHIBufferViewInfo{ m_index_buffer.get() } );

    return true;
}

// CubeAsset

bool CubeAsset::Load( const JsonValue& data )
{
    static std::array<MeshVertex, 8> vertices =
    {
        MeshVertex{.position = { -0.5f, -0.5f, -0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = { -0.5f, -0.5f,  0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = { -0.5f,  0.5f, -0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = { -0.5f,  0.5f,  0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = {  0.5f, -0.5f, -0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = {  0.5f, -0.5f,  0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = {  0.5f,  0.5f, -0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
        MeshVertex{.position = {  0.5f,  0.5f,  0.5f }, .normal = { 0.0f, 0.0f, 0.0f } },
    };

    static std::array<uint16_t, m_cube_indices_count> indices =
    {
        0, 1, 2,
        2, 1, 3,

        4, 6, 5,
        6, 7, 6,

        0, 6, 4,
        0, 2, 6,

        1, 5, 7,
        1, 7, 3,

        0, 4, 5,
        0, 5, 1,

        2, 3, 6,
        3, 7, 6
    };

    if ( ! LoadFromData( vertices, indices ) )
    {
        return false;
    }

    m_status = AssetStatus::Ready;
    return true;
}

// TextureAsset

bool TextureAsset::Load( const JsonValue& data )
{
    JsonValue::ConstMemberIterator source = data.FindMember( "source" );
    if ( source == data.MemberEnd() || !source->value.IsString() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid texture asset: \"source\" value is invalid", m_id.GetPath() );
        return false;
    }

    if ( !LoadFromFile( source->value.GetString() ) )
    {
        return false;
    }

    RHI::TextureROViewInfo view_info = {};
    view_info.texture = m_rhi_texture.get();
    m_rhi_view = GetRHI().CreateTextureROView( view_info );

    m_status = AssetStatus::Ready;
    return true;
}

bool TextureAsset::LoadFromFile( const char* path )
{
    std::string input_file_path = ToOSPath( path );

    // Parse signature
    uint32_t signature;
    {
        std::ifstream file_stream( input_file_path.c_str(), std::ios_base::in | std::ios_base::binary );
        if ( !file_stream.good() )
        {
            SE_LOG_ERROR( Engine, "Could not load texture file at <%s>", input_file_path.c_str() );
            return false;
        }

        file_stream.read( reinterpret_cast<char*>( &signature ), sizeof( signature ) );

        if ( file_stream.eof() || !file_stream.good() )
        {
            SE_LOG_ERROR( Engine, "Could not load file signature at <%s>", input_file_path.c_str() );
            return false;
        }
    }

    static constexpr uint32_t SIGNATURE_EXR = 0x01312F76;
    if ( signature == SIGNATURE_EXR )
    {
        return LoadEXR( input_file_path.c_str() );
    }

    return LoadWithSTB( input_file_path.c_str() );
}

bool TextureAsset::LoadWithSTB( const char* ospath )
{
    bool file_hdr = stbi_is_hdr( ospath );

    if ( file_hdr )
    {
        // requires separate interface, loads floats
        NOTIMPL;
    }

    int file_width, file_height, file_channels;

    stbi_uc* file_pixels = stbi_load( ospath, &file_width, &file_height, &file_channels, STBI_rgb_alpha );
    if ( file_pixels == nullptr )
    {
        SE_LOG_ERROR( Engine, "Could not load texture file at <%s>", ospath );
        return false;
    }

    BOOST_SCOPE_EXIT( file_pixels )
    {
        stbi_image_free( file_pixels );
    } BOOST_SCOPE_EXIT_END

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    if ( file_channels == 4 || file_channels == 3 )
    {
        tex_info.format = RHIFormat::R8G8B8A8_SRGB;
    }
    else
    {
        NOTIMPL;
    }
    tex_info.allow_multiformat_views = false;

    tex_info.width = uint32_t( file_width );
    tex_info.height = uint32_t( file_height );
    tex_info.depth = 1;

    tex_info.mips = 1;
    tex_info.array_layers = 1;

    tex_info.usage = RHITextureUsageFlags::TextureROView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;
    tex_info.initial_queue = RHI::QueueType::Graphics;

    size_t file_data_size = file_width * file_height * RHIUtils::GetRHIFormatSize( tex_info.format );

    m_rhi_texture = RHIUtils::CreateInitializedGPUTexture( tex_info, file_pixels, file_data_size );

    if ( !m_rhi_texture )
    {
        return false;
    }

    m_status = AssetStatus::Ready;
    return true;
}

bool TextureAsset::LoadEXR( const char* ospath )
{
    float* out_pixels = nullptr;
    int width = 0;
    int height = 0;
    const char* err = nullptr;

    int ret = ::LoadEXR( &out_pixels, &width, &height, ospath, &err );

    BOOST_SCOPE_EXIT( out_pixels )
    {
        if ( out_pixels )
        {
            free( out_pixels );
        }
    } BOOST_SCOPE_EXIT_END

    if ( ret != TINYEXR_SUCCESS || out_pixels == nullptr )
    {
        SE_LOG_ERROR( Engine, "Could not load EXR file at <%s>", ospath );
        if ( err )
        {
            SE_LOG_ERROR( Engine, "[TinyEXR]: %s", err );
            FreeEXRErrorMessage( err );
        }

        return false;
    }

    // @todo - convert to RGB9E5

    struct HDRPixelValue
    {
        float r;
        float g;
        float b;
        float a;
    };

    std::vector<HDRPixelValue> pixels_converted;
    pixels_converted.resize( width* height );

    for ( int i = 0; i < width * height; ++i )
    {
        float* file_pixel = &out_pixels[i * 4];
        pixels_converted[i].r = file_pixel[0];
        pixels_converted[i].g = file_pixel[1];
        pixels_converted[i].b = file_pixel[2];
        pixels_converted[i].a = file_pixel[3];
    }
    
    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.format = RHIFormat::RGBA32_SFLOAT;
    
    tex_info.allow_multiformat_views = false;

    tex_info.width = uint32_t( width );
    tex_info.height = uint32_t( height );
    tex_info.depth = 1;

    tex_info.mips = 1;
    tex_info.array_layers = 1;

    tex_info.usage = RHITextureUsageFlags::TextureROView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;
    tex_info.initial_queue = RHI::QueueType::Graphics;

    size_t file_data_size = pixels_converted.size() * RHIUtils::GetRHIFormatSize( tex_info.format );

    m_rhi_texture = RHIUtils::CreateInitializedGPUTexture( tex_info, pixels_converted.data(), file_data_size);

    if ( !m_rhi_texture )
    {
        return false;
    }

    m_status = AssetStatus::Ready;
    return true;
}

// MaterialAsset

bool MaterialAsset::Load( const JsonValue& data )
{
    JsonValue::ConstMemberIterator parms = data.FindMember( "parms" );
    if ( parms == data.MemberEnd() || !parms->value.IsObject() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid material asset: \"parms\" value is invalid", m_id.GetPath() );
        return false;
    }

    bool has_albedo = Serialization::Deserialize( parms->value, "albedo", m_gpu_data.albedo );

    bool has_f0 = Serialization::Deserialize( parms->value, "f0", m_gpu_data.f0 );

    bool has_roughness = Serialization::Deserialize( parms->value, "roughness", m_gpu_data.roughness );

    m_global_material_index = GetRenderer().GetGlobalDescriptors().AddMaterial( m_gpu_data );

    m_status = AssetStatus::Ready;
    return true;
}
