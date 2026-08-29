#include <ThirdParty/ufbx/ufbx.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

static std::string String( ufbx_string value )
{
    return std::string( value.data, value.length );
}

int main( int argc, char** argv )
{
    if ( argc != 2 )
    {
        std::cerr << "Usage: bistro_probe <scene.fbx>\n";
        return 2;
    }

    ufbx_load_opts options = {};
    options.generate_missing_normals = true;
    options.target_axes = ufbx_axes_right_handed_y_up;
    options.target_unit_meters = 1.0;

    ufbx_error error = {};
    ufbx_scene* scene = ufbx_load_file( argv[1], &options, &error );
    if ( !scene )
    {
        char buffer[2048] = {};
        ufbx_format_error( buffer, sizeof( buffer ), &error );
        std::cerr << buffer << '\n';
        return 1;
    }

    size_t mesh_instances = 0;
    size_t triangles = 0;
    size_t material_parts = 0;
    size_t max_mesh_triangles = 0;
    std::vector<uint64_t> triangles_per_material( scene->materials.count + 1 );

    for ( ufbx_node* node : scene->nodes )
    {
        if ( !node->mesh || !node->visible )
            continue;
        ++mesh_instances;
        triangles += node->mesh->num_triangles;
        max_mesh_triangles = std::max( max_mesh_triangles, node->mesh->num_triangles );

        for ( ufbx_mesh_part part : node->mesh->material_parts )
        {
            ++material_parts;
            const uint32_t material_slot = part.index;
            uint32_t material_id = static_cast<uint32_t>( scene->materials.count );
            if ( material_slot < node->materials.count && node->materials[material_slot] )
                material_id = node->materials[material_slot]->typed_id;
            triangles_per_material[material_id] += part.num_triangles;
        }
    }

    std::cout << "nodes=" << scene->nodes.count
        << " meshes=" << scene->meshes.count
        << " instances=" << mesh_instances
        << " triangles=" << triangles
        << " materials=" << scene->materials.count
        << " textures=" << scene->textures.count
        << " material_parts=" << material_parts
        << " max_mesh_triangles=" << max_mesh_triangles << "\n";

    for ( ufbx_material* material : scene->materials )
    {
        const ufbx_material_map& color = material->pbr.base_color;
        const ufbx_material_map& roughness = material->pbr.roughness;
        const ufbx_material_map& metalness = material->pbr.metalness;
        std::string texture_path;
        if ( color.texture )
            texture_path = String( color.texture->relative_filename );

        std::cout << "material[" << material->typed_id << "]"
            << " triangles=" << triangles_per_material[material->typed_id]
            << " name=\"" << String( material->name ) << "\""
            << " base=(" << color.value_vec3.x << ',' << color.value_vec3.y << ',' << color.value_vec3.z << ')'
            << " roughness=" << roughness.value_real
            << " metalness=" << metalness.value_real
            << " texture=\"" << texture_path << "\"\n";
    }

    if ( triangles_per_material.back() > 0 )
        std::cout << "unassigned triangles=" << triangles_per_material.back() << "\n";

    ufbx_free_scene( scene );
    return 0;
}
