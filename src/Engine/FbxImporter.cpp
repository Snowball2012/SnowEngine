#include "StdAfx.h"
#include <Core/CVars.h>
#include <fstream>
#include <filesystem>

extern "C" {
#include <ThirdParty/ufbx/ufbx.h>
}

static std::string JsonEscape(const char *s) { std::string r; for (; s && *s; ++s) { if (*s=='\\' || *s=='"') r += '\\'; r += *s; } return r; }

CONSOLE_COMMAND_DEFINE(import_fbx, "Import an FBX once: import_fbx <file.fbx> <output-name>")
{
    if (args.size() != 2) { output = "usage: import_fbx <source.fbx> <output-name>"; return false; }
    std::filesystem::path source(args[0]);
    if (!std::filesystem::exists(source)) { output = "FBX file not found: " + source.string(); return false; }
    std::string name = args[1];
    for (char &c : name) if (c=='\\' || c=='/' || c==':' || c=='.') c='_';
    if (name.empty()) { output = "output-name is empty"; return false; }
    std::filesystem::path root = std::filesystem::path(g_core_paths.engine_content) / "Imported" / name;
    std::error_code ec; std::filesystem::create_directories(root, ec);
    if (ec) { output = "cannot create output directory: " + ec.message(); return false; }
    ufbx_load_opts opts = {}; opts.generate_missing_normals = true; opts.generate_missing_uvs = true;
    ufbx_error error = {}; ufbx_scene *scene = ufbx_load_file(source.string().c_str(), &opts, &error);
    if (!scene) { output = "ufbx failed: " + std::string(error.description.data, error.description.length); return false; }
    std::ofstream obj(root / (name + ".obj"), std::ios::binary);
    if (!obj) { ufbx_free_scene(scene); output = "cannot write OBJ"; return false; }
    obj << "# SnowEngine FBX import\n";
    size_t vertex_count=0, triangle_count=0;
    for (size_t ni=0; ni<scene->nodes.count; ++ni) {
        ufbx_node *node=scene->nodes.data[ni]; if (!node->mesh) continue;
        ufbx_mesh *mesh=node->mesh;
        obj << "o " << JsonEscape(node->name.data).c_str() << "\n";
        for (size_t i=0;i<mesh->num_vertices;++i) { ufbx_vec3 p=ufbx_transform_position(&node->geometry_to_world, mesh->vertices.data[i]); obj << "v "<<p.x<<" "<<p.y<<" "<<p.z<<"\n"; }
        for (size_t i=0;i<mesh->num_vertices;++i) { ufbx_vec3 n=mesh->vertex_normal.data ? mesh->vertex_normal.data[i] : ufbx_vec3{0,1,0}; obj << "vn "<<n.x<<" "<<n.y<<" "<<n.z<<"\n"; }
        for (size_t fi=0;fi<mesh->num_faces;++fi) { ufbx_face f=mesh->faces.data[fi]; std::vector<uint32_t> tri(f.num_indices*3); uint32_t nt=ufbx_triangulate_face(tri.data(),tri.size(),mesh,f); for(uint32_t t=0;t<nt;++t){ obj<<"f "; for(int k=0;k<3;++k){ uint32_t v=tri[t*3+k]+uint32_t(vertex_count)+1; obj<<v<<"//"<<v<<(k==2?'\n':' '); } ++triangle_count; } }
        vertex_count += mesh->num_vertices;
    }
    obj.close();
    std::ofstream sea(root / (name + ".sea")); sea << "{\n  \"_generator\": \"MeshAsset\",\n  \"source\": \"#engine/Imported/"<<name<<"/"<<name<<".obj\",\n  \"material\": \"#engine/Materials/Default.sea\"\n}\n";
    std::ofstream mat(root / "Default.sea"); mat << "{\n  \"_generator\": \"MaterialAsset\",\n  \"base_color\": [1.0, 1.0, 1.0, 1.0],\n  \"roughness\": 0.5,\n  \"metallic\": 0.0\n}\n";
    std::ofstream level(root / (name + ".sel")); level << "{\n  \"_generator\": \"LevelAsset\",\n  \"objects\": [{\"name\": \""<<name<<"\", \"traits\": [{\"_type\": \"MeshInstanceTrait\", \"value\": {\"asset\": \"#engine/Imported/"<<name<<"/"<<name<<".sea\"}}]}]\n}\n";
    ufbx_free_scene(scene); output = "Imported " + std::to_string(vertex_count) + " vertices / " + std::to_string(triangle_count) + " triangles to " + root.string(); return true;
}
