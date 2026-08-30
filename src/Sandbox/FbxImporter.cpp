#include "StdAfx.h"

#include <Core/CVars.h>

#include <ufbx/ufbx.h>

namespace
{
    namespace fs = std::filesystem;

    struct ImportVertex
    {
        ufbx_vec3 position;
        ufbx_vec3 normal;
        ufbx_vec2 uv;
    };

    struct ImportVertexKey
    {
        double values[8];
        bool operator==( const ImportVertexKey& rhs ) const = default;
    };

    struct ImportVertexKeyHash
    {
        size_t operator()( const ImportVertexKey& key ) const
        {
            size_t hash = 0;
            const auto combine = [&]( double value )
            {
                hash ^= std::hash<double>{}( value ) + 0x9e3779b9 + ( hash << 6 ) + ( hash >> 2 );
            };
            for ( double value : key.values )
                combine( value );
            return hash;
        }
    };

    ImportVertexKey MakeVertexKey( const ImportVertex& vertex )
    {
        return { {
            vertex.position.x, vertex.position.y, vertex.position.z,
            vertex.normal.x, vertex.normal.y, vertex.normal.z,
            vertex.uv.x, vertex.uv.y,
        } };
    }

    struct MaterialGroup
    {
        ufbx_material* material = nullptr;
        std::string name;
        std::vector<ImportVertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<ImportVertexKey, uint32_t, ImportVertexKeyHash> vertex_map;
    };

    std::string ToString( ufbx_string value )
    {
        return std::string( value.data, value.length );
    }

    std::string SanitizeName( std::string value )
    {
        for ( char& c : value )
        {
            const unsigned char byte = static_cast<unsigned char>( c );
            if ( !std::isalnum( byte ) && c != '-' && c != '_' )
                c = '_';
        }
        while ( !value.empty() && value.back() == '_' )
            value.pop_back();
        return value.empty() ? "Unnamed" : value;
    }

    std::string JsonEscape( const std::string& value )
    {
        std::string result;
        result.reserve( value.size() );
        for ( char c : value )
        {
            switch ( c )
            {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
            }
        }
        return result;
    }

    ufbx_vec3 Normalize( ufbx_vec3 value )
    {
        const double length_sq = value.x * value.x + value.y * value.y + value.z * value.z;
        if ( length_sq <= 1.e-20 )
            return { 0.0, 1.0, 0.0 };
        const double inverse_length = 1.0 / sqrt( length_sq );
        return { value.x * inverse_length, value.y * inverse_length, value.z * inverse_length };
    }

    bool WriteTextFile( const fs::path& path, const std::function<void( std::ostream& )>& write )
    {
        std::ofstream file( path, std::ios::binary );
        if ( !file )
            return false;
        file << std::setprecision( 9 );
        write( file );
        return file.good();
    }

    bool PathStartsWith( const fs::path& child, const fs::path& parent )
    {
        auto child_it = child.begin();
        for ( auto parent_it = parent.begin(); parent_it != parent.end(); ++parent_it, ++child_it )
        {
            if ( child_it == child.end() || *child_it != *parent_it )
                return false;
        }
        return true;
    }

    fs::path ResolveTexturePath( const fs::path& source_directory, ufbx_texture* texture, const std::string& fallback_name )
    {
        std::vector<fs::path> candidates;
        if ( texture )
        {
            const std::string relative = ToString( texture->relative_filename );
            const std::string absolute = ToString( texture->absolute_filename );
            if ( !relative.empty() )
                candidates.emplace_back( source_directory / fs::path( relative ) );
            if ( !absolute.empty() )
                candidates.emplace_back( fs::path( absolute ) );
        }
        if ( !fallback_name.empty() )
            candidates.emplace_back( source_directory / "Textures" / fallback_name );

        std::error_code ec;
        const fs::path allowed_root = fs::weakly_canonical( source_directory, ec );
        for ( const fs::path& candidate : candidates )
        {
            ec.clear();
            const fs::path canonical = fs::weakly_canonical( candidate, ec );
            if ( !ec && fs::is_regular_file( canonical ) && PathStartsWith( canonical, allowed_root ) )
                return canonical;
        }
        return {};
    }

    void AppendMeshPart( MaterialGroup& group, const ufbx_node& node, const ufbx_mesh_part& part )
    {
        const ufbx_mesh& mesh = *node.mesh;
        const ufbx_matrix normal_matrix = ufbx_matrix_for_normals( &node.geometry_to_world );
        std::vector<uint32_t> triangle_indices( mesh.max_face_triangles * 3 );

        for ( uint32_t face_index : part.face_indices )
        {
            const ufbx_face face = mesh.faces[face_index];
            const uint32_t num_triangles = ufbx_triangulate_face(
                triangle_indices.data(), triangle_indices.size(), &mesh, face );
            for ( uint32_t triangle = 0; triangle < num_triangles; ++triangle )
            {
                for ( uint32_t corner = 0; corner < 3; ++corner )
                {
                    const uint32_t vertex_index = triangle_indices[triangle * 3 + corner];
                    ImportVertex vertex = {};
                    vertex.position = ufbx_transform_position(
                        &node.geometry_to_world, ufbx_get_vertex_vec3( &mesh.vertex_position, vertex_index ) );
                    if ( mesh.vertex_normal.exists )
                    {
                        vertex.normal = Normalize( ufbx_transform_direction(
                            &normal_matrix, ufbx_get_vertex_vec3( &mesh.vertex_normal, vertex_index ) ) );
                    }
                    else
                    {
                        vertex.normal = { 0.0, 1.0, 0.0 };
                    }
                    if ( mesh.vertex_uv.exists )
                    {
                        vertex.uv = ufbx_get_vertex_vec2( &mesh.vertex_uv, vertex_index );
                        // FBX UVs use a bottom-left origin; engine texture data uses top-left.
                        vertex.uv.y = 1.0 - vertex.uv.y;
                    }

                    const ImportVertexKey vertex_key = MakeVertexKey( vertex );
                    auto existing = group.vertex_map.find( vertex_key );
                    if ( existing != group.vertex_map.end() )
                    {
                        group.indices.push_back( existing->second );
                        continue;
                    }

                    const uint32_t new_index = static_cast<uint32_t>( group.vertices.size() );
                    group.vertices.push_back( vertex );
                    group.indices.push_back( new_index );
                    group.vertex_map.emplace( vertex_key, new_index );
                }
            }
        }
    }

    bool WriteObj( const fs::path& path, const MaterialGroup& group )
    {
        return WriteTextFile( path, [&]( std::ostream& file )
        {
            file << "# Generated once by SnowEngine import_fbx\n";
            for ( const ImportVertex& vertex : group.vertices )
                file << "v " << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z << '\n';
            for ( const ImportVertex& vertex : group.vertices )
                file << "vt " << vertex.uv.x << ' ' << vertex.uv.y << '\n';
            for ( const ImportVertex& vertex : group.vertices )
                file << "vn " << vertex.normal.x << ' ' << vertex.normal.y << ' ' << vertex.normal.z << '\n';
            for ( size_t index = 0; index < group.indices.size(); index += 3 )
            {
                file << "f";
                for ( size_t corner = 0; corner < 3; ++corner )
                {
                    const uint32_t obj_index = group.indices[index + corner] + 1;
                    file << ' ' << obj_index << '/' << obj_index << '/' << obj_index;
                }
                file << '\n';
            }
        } );
    }

    std::string CopyTextureAsset(
        const fs::path& source_directory,
        const fs::path& output_directory,
        const std::string& output_name,
        ufbx_texture* texture,
        const std::string& fallback_name,
        bool srgb )
    {
        const fs::path source = ResolveTexturePath( source_directory, texture, fallback_name );
        if ( source.empty() )
            return {};

        const fs::path texture_directory = output_directory / "Textures";
        std::error_code ec;
        fs::create_directories( texture_directory, ec );
        if ( ec )
            return {};

        const fs::path destination = texture_directory / source.filename();
        fs::copy_file( source, destination, fs::copy_options::overwrite_existing, ec );
        if ( ec )
            return {};

        const std::string wrapper_name = SanitizeName( destination.stem().string() ) + ".sea";
        const std::string source_asset = "#engine/Imported/" + output_name + "/Textures/" + destination.filename().generic_string();
        if ( !WriteTextFile( texture_directory / wrapper_name, [&]( std::ostream& file )
            {
                file << "{\n  \"_generator\": \"TextureAsset\",\n"
                    << "  \"source\": \"" << JsonEscape( source_asset ) << "\",\n"
                    << "  \"srgb\": " << ( srgb ? "true" : "false" ) << "\n}\n";
            } ) )
        {
            return {};
        }
        return "#engine/Imported/" + output_name + "/Textures/" + wrapper_name;
    }

    bool WriteMaterialAndMeshAssets(
        const fs::path& source_directory,
        const fs::path& output_directory,
        const std::string& output_name,
        const MaterialGroup& group )
    {
        const ufbx_material_map* base_map = group.material ? &group.material->pbr.base_color : nullptr;
        const ufbx_material_map* roughness_map = group.material ? &group.material->pbr.roughness : nullptr;
        const ufbx_material_map* metalness_map = group.material ? &group.material->pbr.metalness : nullptr;
        const ufbx_material_map* normal_map = group.material ? &group.material->pbr.normal_map : nullptr;
        const ufbx_material_map* emission_color_map = group.material ? &group.material->pbr.emission_color : nullptr;
        const ufbx_material_map* emission_factor_map = group.material ? &group.material->pbr.emission_factor : nullptr;

        ufbx_vec3 base_color = base_map && base_map->has_value ? base_map->value_vec3 : ufbx_vec3{ 1.0, 1.0, 1.0 };
        const double roughness = roughness_map && roughness_map->has_value
            ? std::clamp<double>( roughness_map->value_real, 0.02, 1.0 ) : 0.5;
        const double metalness = metalness_map && metalness_map->has_value
            ? std::clamp<double>( metalness_map->value_real, 0.0, 1.0 ) : 0.0;

        const std::string material_file_base = group.material ? ToString( group.material->name ) : group.name;
        const std::string albedo_texture = CopyTextureAsset(
            source_directory, output_directory, output_name,
            base_map ? base_map->texture : nullptr, material_file_base + "_BaseColor.dds", true );
        const std::string specular_texture = CopyTextureAsset(
            source_directory, output_directory, output_name,
            roughness_map ? roughness_map->texture : nullptr, material_file_base + "_Specular.dds", false );
        const std::string normal_texture = CopyTextureAsset(
            source_directory, output_directory, output_name,
            normal_map ? normal_map->texture : nullptr, material_file_base + "_Normal.dds", false );
        ufbx_texture* emission_source = emission_color_map && emission_color_map->texture
            ? emission_color_map->texture : ( emission_factor_map ? emission_factor_map->texture : nullptr );
        const std::string emissive_texture = CopyTextureAsset(
            source_directory, output_directory, output_name,
            emission_source, material_file_base + "_Emissive.dds", true );

        ufbx_vec3 emissive = emission_color_map && emission_color_map->has_value
            ? emission_color_map->value_vec3 : ufbx_vec3{ 0.0, 0.0, 0.0 };
        double emission_factor = emission_factor_map && emission_factor_map->has_value
            ? emission_factor_map->value_real : 1.0;
        if ( !emissive_texture.empty() && ( !emission_color_map || !emission_color_map->has_value ) )
            emissive = { 1.0, 1.0, 1.0 };
        emissive.x *= emission_factor;
        emissive.y *= emission_factor;
        emissive.z *= emission_factor;

        const ufbx_vec3 diffuse_color = {
            base_color.x * ( 1.0 - metalness ),
            base_color.y * ( 1.0 - metalness ),
            base_color.z * ( 1.0 - metalness ),
        };
        const ufbx_vec3 f0 = {
            0.04 * ( 1.0 - metalness ) + base_color.x * metalness,
            0.04 * ( 1.0 - metalness ) + base_color.y * metalness,
            0.04 * ( 1.0 - metalness ) + base_color.z * metalness,
        };

        const fs::path material_path = output_directory / ( group.name + ".material.sea" );
        if ( !WriteTextFile( material_path, [&]( std::ostream& file )
            {
                file << "{\n  \"_generator\": \"MaterialAsset\",\n  \"parms\": {\n"
                    << "    \"roughness\": " << roughness << ",\n"
                    << "    \"albedo\": [" << diffuse_color.x << ", " << diffuse_color.y << ", " << diffuse_color.z << "],\n"
                    << "    \"f0\": [" << f0.x << ", " << f0.y << ", " << f0.z << "],\n"
                    << "    \"emissive\": [" << emissive.x << ", " << emissive.y << ", " << emissive.z << "]";
                if ( !albedo_texture.empty() )
                    file << ",\n    \"albedo_texture\": \"" << JsonEscape( albedo_texture ) << "\"";
                if ( !specular_texture.empty() )
                    file << ",\n    \"specular_texture\": \"" << JsonEscape( specular_texture ) << "\"";
                if ( !normal_texture.empty() )
                    file << ",\n    \"normal_texture\": \"" << JsonEscape( normal_texture ) << "\"";
                if ( !emissive_texture.empty() )
                    file << ",\n    \"emissive_texture\": \"" << JsonEscape( emissive_texture ) << "\"";
                file << "\n  }\n}\n";
            } ) )
        {
            return false;
        }

        const std::string obj_asset = "#engine/Imported/" + output_name + "/" + group.name + ".obj";
        const std::string material_asset = "#engine/Imported/" + output_name + "/" + group.name + ".material.sea";
        return WriteTextFile( output_directory / ( group.name + ".mesh.sea" ), [&]( std::ostream& file )
        {
            file << "{\n  \"_generator\": \"MeshAsset\",\n"
                << "  \"source\": \"" << JsonEscape( obj_asset ) << "\",\n"
                << "  \"material\": \"" << JsonEscape( material_asset ) << "\"\n}\n";
        } );
    }

    bool WriteLevel(
        const fs::path& output_directory,
        const std::string& output_name,
        const std::vector<MaterialGroup>& groups )
    {
        return WriteTextFile( output_directory / ( output_name + ".sel" ), [&]( std::ostream& file )
        {
            file << "{\n  \"_generator\": \"LevelAsset\",\n  \"objects\": [\n";
            bool first = true;
            for ( const MaterialGroup& group : groups )
            {
                if ( group.indices.empty() )
                    continue;
                if ( !first )
                    file << ",\n";
                first = false;
                const std::string mesh_asset = "#engine/Imported/" + output_name + "/" + group.name + ".mesh.sea";
                file << "    {\n      \"name\": \"" << JsonEscape( group.name ) << "\",\n      \"traits\": [\n"
                    << "        {\n          \"_type\": \"TransformTrait\",\n          \"value\": {\n"
                    << "            \"tf\": { \"t\": [0.0, 0.0, 0.0], \"o\": [0.0, 0.0, 0.0, 1.0], \"s\": [1.0, 1.0, 1.0] },\n"
                    << "            \"euler\": [0.0, 0.0, 0.0]\n          }\n        },\n"
                    << "        {\n          \"_type\": \"MeshInstanceTrait\",\n          \"value\": { \"asset\": \""
                    << JsonEscape( mesh_asset ) << "\" }\n        }\n"
                    << "      ]\n    }";
            }
            file << "\n  ]\n}\n";
        } );
    }

    std::vector<MaterialGroup> BuildMaterialGroups( ufbx_scene& scene, size_t& triangle_count )
    {
        std::vector<MaterialGroup> groups( scene.materials.count + 1 );
        for ( ufbx_material* material : scene.materials )
        {
            MaterialGroup& group = groups[material->typed_id];
            group.material = material;
            group.name = SanitizeName( ToString( material->name ) ) + "_" + std::to_string( material->typed_id );
        }
        groups.back().name = "Default";

        triangle_count = 0;
        for ( ufbx_node* node : scene.nodes )
        {
            if ( !node->mesh || !node->visible )
                continue;
            for ( const ufbx_mesh_part& part : node->mesh->material_parts )
            {
                size_t group_index = scene.materials.count;
                if ( part.index < node->materials.count && node->materials[part.index] )
                    group_index = node->materials[part.index]->typed_id;
                AppendMeshPart( groups[group_index], *node, part );
                triangle_count += part.num_triangles;
            }
        }
        return groups;
    }

    bool ConvertFbx(
        const fs::path& source,
        const std::string& requested_name,
        bool force,
        std::string& result )
    {
        std::error_code ec;
        const fs::path source_path = fs::weakly_canonical( source, ec );
        if ( ec || !fs::is_regular_file( source_path ) )
        {
            result = "FBX file not found: " + source.string();
            return false;
        }

        const std::string output_name = SanitizeName( requested_name );
        const fs::path imported_root = fs::weakly_canonical(
            fs::path( g_core_paths.engine_content ) / "Imported", ec );
        if ( ec )
        {
            fs::create_directories( fs::path( g_core_paths.engine_content ) / "Imported", ec );
            ec.clear();
        }
        const fs::path output_directory = fs::path( g_core_paths.engine_content ) / "Imported" / output_name;
        if ( fs::exists( output_directory ) )
        {
            if ( !force )
            {
                result = "Output already exists; use --force to replace it: " + output_directory.string();
                return false;
            }
            const fs::path canonical_output = fs::weakly_canonical( output_directory, ec );
            if ( ec || canonical_output == imported_root || !PathStartsWith( canonical_output, imported_root ) )
            {
                result = "Refusing to replace output outside EngineContent/Imported";
                return false;
            }
            fs::remove_all( canonical_output, ec );
            if ( ec )
            {
                result = "Could not replace existing output: " + ec.message();
                return false;
            }
        }
        fs::create_directories( output_directory, ec );
        if ( ec )
        {
            result = "Could not create output directory: " + ec.message();
            return false;
        }

        ufbx_load_opts options = {};
        options.generate_missing_normals = true;
        options.target_axes = ufbx_axes_right_handed_y_up;
        options.target_unit_meters = 1.0;

        ufbx_error error = {};
        ufbx_scene* scene = ufbx_load_file( source_path.string().c_str(), &options, &error );
        if ( !scene )
        {
            char error_buffer[2048] = {};
            ufbx_format_error( error_buffer, sizeof( error_buffer ), &error );
            result = "ufbx could not load the scene: " + std::string( error_buffer );
            return false;
        }

        size_t triangle_count = 0;
        std::vector<MaterialGroup> groups = BuildMaterialGroups( *scene, triangle_count );
        const size_t group_count = std::count_if( groups.begin(), groups.end(),
            []( const MaterialGroup& group ) { return !group.indices.empty(); } );
        if ( group_count > 256 )
        {
            ufbx_free_scene( scene );
            result = "Scene requires " + std::to_string( group_count )
                + " material geometry groups, but the renderer supports 256";
            return false;
        }

        size_t vertex_count = 0;
        for ( MaterialGroup& group : groups )
        {
            if ( group.indices.empty() )
                continue;
            vertex_count += group.vertices.size();
            if ( !WriteObj( output_directory / ( group.name + ".obj" ), group )
                || !WriteMaterialAndMeshAssets(
                    source_path.parent_path(), output_directory, output_name, group ) )
            {
                ufbx_free_scene( scene );
                result = "Failed while writing material group " + group.name
                    + "; partial output remains and can be replaced with --force";
                return false;
            }
            group.vertex_map.clear();
            group.vertex_map.rehash( 0 );
        }

        if ( !WriteLevel( output_directory, output_name, groups ) )
        {
            ufbx_free_scene( scene );
            result = "Could not write imported level";
            return false;
        }

        ufbx_free_scene( scene );
        result = "Imported " + std::to_string( triangle_count ) + " triangles, "
            + std::to_string( vertex_count ) + " vertices and "
            + std::to_string( group_count ) + " material groups. Start level: #engine/Imported/"
            + output_name + "/" + output_name + ".sel";
        return true;
    }
}

CONSOLE_COMMAND_DEFINE( import_fbx,
    "Import an FBX once: import_fbx <source.fbx> <output-name> [--force]" )
{
    if ( args.size() < 2 || args.size() > 3 || ( args.size() == 3 && args[2] != "--force" ) )
    {
        output = "usage: import_fbx <source.fbx> <output-name> [--force]";
        return false;
    }
    return ConvertFbx( fs::path( args[0] ), args[1], args.size() == 3, output );
}
