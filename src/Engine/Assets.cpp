
#include "StdAfx.h"

#include "Assets.h"

#include "RHIUtils.h"
#include "Scene.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#define BCDEC_IMPLEMENTATION
#include <bcdec/bcdec.h>

#include <stb/stb_image.h>

// MeshAsset

struct MeshVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
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
    return RHIIndexBufferType::UInt32;
}

bool MeshAsset::Load( const JsonValue& data )
{
    JsonValue::ConstMemberIterator source = data.FindMember( "source" );
    if ( source == data.MemberEnd() || !source->value.IsString() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid mesh asset: \"source\" value is invalid", m_id.GetPath() );
        return false;
    }

    const auto load_start = std::chrono::steady_clock::now();
    SE_LOG_INFO( Engine, "Loading mesh asset %s from %s", m_id.GetPath(), source->value.GetString() );
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
    const auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - load_start ).count();
    SE_LOG_INFO( Engine, "Loaded mesh asset %s (%u indices) in %lld ms",
        m_id.GetPath(), m_indices_num, static_cast<long long>( load_ms ) );
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

    struct VertexKey
    {
        int position;
        int normal;
        int texcoord;
        bool operator==( const VertexKey& rhs ) const = default;
    };
    struct VertexKeyHash
    {
        size_t operator()( const VertexKey& key ) const
        {
            size_t hash = std::hash<int>{}( key.position );
            hash ^= std::hash<int>{}( key.normal ) + 0x9e3779b9 + ( hash << 6 ) + ( hash >> 2 );
            hash ^= std::hash<int>{}( key.texcoord ) + 0x9e3779b9 + ( hash << 6 ) + ( hash >> 2 );
            return hash;
        }
    };
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertex_attribs_to_index;

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    // vertices array stores 3 elements per vertex
    vertices.reserve( attrib.vertices.size() / 3 );
    if ( attrib.vertices.size() % 3 != 0 )
    {
        SE_LOG_ERROR( Engine, "Obj file vertex array size not multiple of 3. File: <%s> ", input_file_path.c_str() );
        return false;
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

                const VertexKey vertex_key = { idx.vertex_index, idx.normal_index, idx.texcoord_index };
                
                uint32_t vertex_idx = uint32_t(-1);
                auto existing_index_it = vertex_attribs_to_index.find( vertex_key );
                if ( existing_index_it != vertex_attribs_to_index.end() )
                {
                    vertex_idx = existing_index_it->second;
                }
                else
                {
                    vertex_idx = uint32_t( vertices.size() );

                    MeshVertex new_vertex = {};

                    const int pos_offset = idx.vertex_index * 3;
                    tinyobj::real_t vx = attrib.vertices[pos_offset + 0];
                    tinyobj::real_t vy = attrib.vertices[pos_offset + 1];
                    tinyobj::real_t vz = attrib.vertices[pos_offset + 2];

                    new_vertex.position = glm::vec3( vx, vy, vz );
                    
                    const int normal_offset = idx.normal_index * 3;
                    if ( normal_offset >= 0 )
                    {
                        tinyobj::real_t nx = attrib.normals[normal_offset + 0];
                        tinyobj::real_t ny = attrib.normals[normal_offset + 1];
                        tinyobj::real_t nz = attrib.normals[normal_offset + 2];

                        new_vertex.normal = glm::vec3( nx, ny, nz );
                    }

                    const int texcoord_offset = idx.texcoord_index * 2;
                    if ( texcoord_offset >= 0 )
                    {
                        tinyobj::real_t tu = attrib.texcoords[texcoord_offset + 0];
                        tinyobj::real_t tv = attrib.texcoords[texcoord_offset + 1];

                        new_vertex.uv = glm::vec2( tu, tv );
                    }

                    vertices.emplace_back( new_vertex );
                    vertex_attribs_to_index.emplace( vertex_key, vertex_idx );
                }

                indices.emplace_back( vertex_idx );
            }
            index_offset += fv;
        }
    }

    return LoadFromData( vertices, indices );
}

bool MeshAsset::LoadFromData( const std::span<MeshVertex>& vertices, const std::span<uint32_t>& indices )
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

    static std::array<uint32_t, m_cube_indices_count> indices =
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

    bool srgb = true;
    JsonValue::ConstMemberIterator srgb_it = data.FindMember( "srgb" );
    if ( srgb_it != data.MemberEnd() && srgb_it->value.IsBool() )
    {
        srgb = srgb_it->value.GetBool();
    }

    const auto load_start = std::chrono::steady_clock::now();
    SE_LOG_INFO( Engine, "Loading texture asset %s from %s", m_id.GetPath(), source->value.GetString() );
    if ( !LoadFromFile( source->value.GetString(), srgb ) )
    {
        return false;
    }

    RHI::TextureROViewInfo view_info = {};
    view_info.texture = m_rhi_texture.get();
    m_rhi_view = GetRHI().CreateTextureROView( view_info );
    m_global_texture_index = GetRenderer().GetGlobalDescriptors().AddTexture( *m_rhi_view );
    if ( m_global_texture_index == uint32_t( -1 ) )
    {
        SE_LOG_ERROR( Engine, "Global texture descriptor capacity exhausted while loading %s", m_id.GetPath() );
        return false;
    }

    m_status = AssetStatus::Ready;
    const auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - load_start ).count();
    SE_LOG_INFO( Engine, "Loaded texture asset %s in %lld ms",
        m_id.GetPath(), static_cast<long long>( load_ms ) );
    return true;
}

bool TextureAsset::LoadFromFile( const char* path, bool srgb )
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

    static constexpr uint32_t SIGNATURE_DDS = 0x20534444;
    if ( signature == SIGNATURE_DDS )
    {
        return LoadDDS( input_file_path.c_str(), srgb );
    }

    return LoadWithSTB( input_file_path.c_str(), srgb );
}

bool TextureAsset::LoadWithSTB( const char* ospath, bool srgb )
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
        tex_info.format = srgb ? RHIFormat::R8G8B8A8_SRGB : RHIFormat::R8G8B8A8_UNORM;
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

bool TextureAsset::LoadDDS( const char* ospath, bool srgb )
{
    std::ifstream file( ospath, std::ios::binary | std::ios::ate );
    if ( !file )
    {
        SE_LOG_ERROR( Engine, "Could not load DDS file at <%s>", ospath );
        return false;
    }

    const std::streamsize file_size = file.tellg();
    if ( file_size < 128 )
    {
        SE_LOG_ERROR( Engine, "DDS file is truncated: <%s>", ospath );
        return false;
    }
    file.seekg( 0 );
    std::vector<uint8_t> bytes( static_cast<size_t>( file_size ) );
    file.read( reinterpret_cast<char*>( bytes.data() ), file_size );
    if ( !file )
    {
        SE_LOG_ERROR( Engine, "Could not read DDS file at <%s>", ospath );
        return false;
    }

    auto read_u32 = [&]( size_t offset )
    {
        uint32_t value = 0;
        memcpy( &value, bytes.data() + offset, sizeof( value ) );
        return value;
    };

    const uint32_t width = read_u32( 16 );
    const uint32_t height = read_u32( 12 );
    const uint32_t fourcc = read_u32( 84 );
    const uint32_t dxt1 = uint32_t( 'D' ) | ( uint32_t( 'X' ) << 8 ) | ( uint32_t( 'T' ) << 16 ) | ( uint32_t( '1' ) << 24 );
    const uint32_t dxt5 = uint32_t( 'D' ) | ( uint32_t( 'X' ) << 8 ) | ( uint32_t( 'T' ) << 16 ) | ( uint32_t( '5' ) << 24 );
    const uint32_t ati2 = uint32_t( 'A' ) | ( uint32_t( 'T' ) << 8 ) | ( uint32_t( 'I' ) << 16 ) | ( uint32_t( '2' ) << 24 );
    const uint32_t block_size = fourcc == dxt1 ? 8u : ( fourcc == dxt5 || fourcc == ati2 ? 16u : 0u );
    if ( width == 0 || height == 0 || block_size == 0 )
    {
        SE_LOG_ERROR( Engine, "Unsupported DDS format in <%s>; expected BC1/DXT1, BC3/DXT5 or BC5/ATI2", ospath );
        return false;
    }

    const uint32_t blocks_x = ( width + 3 ) / 4;
    const uint32_t blocks_y = ( height + 3 ) / 4;
    const size_t compressed_size = size_t( blocks_x ) * blocks_y * block_size;
    if ( bytes.size() < 128 + compressed_size )
    {
        SE_LOG_ERROR( Engine, "DDS top mip is truncated: <%s>", ospath );
        return false;
    }

    std::vector<uint8_t> pixels( size_t( width ) * height * 4, 255 );
    const uint8_t* src = bytes.data() + 128;
    for ( uint32_t by = 0; by < blocks_y; ++by )
    {
        for ( uint32_t bx = 0; bx < blocks_x; ++bx, src += block_size )
        {
            uint8_t rgba[4 * 4 * 4] = {};
            if ( fourcc == dxt1 )
            {
                bcdec_bc1( src, rgba, 4 * 4 );
            }
            else if ( fourcc == dxt5 )
            {
                bcdec_bc3( src, rgba, 4 * 4 );
            }
            else
            {
                uint8_t rg[4 * 4 * 2] = {};
                bcdec_bc5( src, rg, 4 * 2 );
                for ( uint32_t i = 0; i < 16; ++i )
                {
                    const float nx = float( rg[i * 2 + 0] ) / 127.5f - 1.0f;
                    const float ny = float( rg[i * 2 + 1] ) / 127.5f - 1.0f;
                    const float nz = sqrtf( std::max( 0.0f, 1.0f - nx * nx - ny * ny ) );
                    rgba[i * 4 + 0] = rg[i * 2 + 0];
                    rgba[i * 4 + 1] = rg[i * 2 + 1];
                    rgba[i * 4 + 2] = uint8_t( nz * 255.0f + 0.5f );
                    rgba[i * 4 + 3] = 255;
                }
            }

            for ( uint32_t y = 0; y < 4 && by * 4 + y < height; ++y )
            {
                for ( uint32_t x = 0; x < 4 && bx * 4 + x < width; ++x )
                {
                    const size_t dst_pixel = ( size_t( by * 4 + y ) * width + bx * 4 + x ) * 4;
                    memcpy( pixels.data() + dst_pixel, rgba + ( y * 4 + x ) * 4, 4 );
                }
            }
        }
    }

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.format = srgb ? RHIFormat::R8G8B8A8_SRGB : RHIFormat::R8G8B8A8_UNORM;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.usage = RHITextureUsageFlags::TextureROView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;
    tex_info.initial_queue = RHI::QueueType::Graphics;
    m_rhi_texture = RHIUtils::CreateInitializedGPUTexture( tex_info, pixels.data(), pixels.size() );
    return m_rhi_texture != nullptr;
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
        SE_LOG_ERROR( Engine, "File %s is not a valid material: \"parms\" is missing or not an object", m_id.GetPath() );
        return false;
    }

    MaterialDescription description;
    const bool has_albedo = Serialization::Deserialize( parms->value, "albedo", description.albedo );
    const bool has_f0 = Serialization::Deserialize( parms->value, "f0", description.f0 );
    const bool has_roughness = Serialization::Deserialize( parms->value, "roughness", description.roughness );
    Serialization::Deserialize( parms->value, "emissive", description.emissive );
    if ( !has_albedo || !has_f0 || !has_roughness )
    {
        SE_LOG_ERROR( Engine, "File %s is missing valid albedo, f0, or roughness values", m_id.GetPath() );
        return false;
    }

    auto read_texture = [&]( const char* property, std::string& destination )
    {
        auto it = parms->value.FindMember( property );
        if ( it == parms->value.MemberEnd() )
            return true;
        if ( !it->value.IsString() )
            return false;
        destination = it->value.GetString();
        return true;
    };

    if ( !read_texture( "albedo_texture", description.albedo_texture ) ||
        !read_texture( "specular_texture", description.specular_texture ) ||
        !read_texture( "normal_texture", description.normal_texture ) ||
        !read_texture( "emissive_texture", description.emissive_texture ) )
    {
        SE_LOG_ERROR( Engine, "File %s contains a material texture path that is not a string", m_id.GetPath() );
        return false;
    }
    return ApplyDescription( description );
}

bool MaterialAsset::ApplyDescription( const MaterialDescription& description )
{
    const auto finite_vector = []( const glm::vec3& value )
    {
        return std::isfinite( value.x ) && std::isfinite( value.y ) && std::isfinite( value.z );
    };
    if ( !finite_vector( description.albedo ) || !finite_vector( description.f0 ) ||
        !finite_vector( description.emissive ) || !std::isfinite( description.roughness ) ||
        description.roughness < 0.02f || description.roughness > 1.0f )
    {
        SE_LOG_ERROR( Engine, "Material %s contains invalid numeric values", m_id.GetPath() );
        return false;
    }

    MaterialGPU gpu = {};
    gpu.albedo = description.albedo;
    gpu.f0 = description.f0;
    gpu.roughness = description.roughness;
    gpu.emissive = description.emissive;

    TextureAssetPtr textures[4] = {};
    uint32_t* indices[4] = { &gpu.albedo_texture_index, &gpu.specular_texture_index, &gpu.normal_texture_index, &gpu.emissive_texture_index };
    const std::string* paths[4] = { &description.albedo_texture, &description.specular_texture, &description.normal_texture, &description.emissive_texture };
    for ( int i = 0; i < 4; ++i )
    {
        if ( paths[i]->empty() )
            continue;
        textures[i] = LoadAsset<TextureAsset>( paths[i]->c_str() );
        if ( textures[i] == nullptr )
        {
            SE_LOG_ERROR( Engine, "Could not load texture %s for material %s", paths[i]->c_str(), m_id.GetPath() );
            return false;
        }
        *indices[i] = textures[i]->GetGlobalTextureIndex();
    }

    if ( m_global_material_index == uint32_t( -1 ) )
    {
        m_global_material_index = GetRenderer().GetGlobalDescriptors().AddMaterial( gpu );
        if ( m_global_material_index == uint32_t( -1 ) )
        {
            SE_LOG_ERROR( Engine, "Global material capacity exhausted while loading %s", m_id.GetPath() );
            return false;
        }
    }

    m_description = description;
    m_gpu_data = gpu;
    m_albedo_texture = std::move( textures[0] );
    m_specular_texture = std::move( textures[1] );
    m_normal_texture = std::move( textures[2] );
    m_emissive_texture = std::move( textures[3] );
    m_status = AssetStatus::Ready;
    return true;
}
