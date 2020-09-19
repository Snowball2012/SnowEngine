// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <engine/stdafx.h>

#include "SceneImporter.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <fbxsdk.h>

namespace
{
    class FbxSceneVisitor
    {
    public:
        virtual ~FbxSceneVisitor() {}
        virtual bool VisitNode( FbxNode* node, bool& visit_children ) = 0;
    };

    class FbxSceneTraverser
    {
    public:
        bool TraverseFbxScene( const FbxScene& scene, FbxSceneVisitor& visitor )
        {
            FbxNode* root_node = scene.GetRootNode();
            if ( ! root_node )
                return false;

            for ( int i = 0; i < root_node->GetChildCount(); ++i )
            {
                auto* child = root_node->GetChild( i );
                if ( ! child )
                    return false;
                if ( ! TraverseNode( child, visitor ) )
                    return false;
            }

            return true;
        }
    private:
        bool TraverseNode( FbxNode* node, FbxSceneVisitor& visitor )
        {
            bool visit_children = true;
            if ( ! visitor.VisitNode( node, visit_children ) )
                return false;
            
            if ( visit_children )
            {
                for ( int i = 0; i < node->GetChildCount(); ++i )
                {
                    auto* child = node->GetChild( i );
                    if ( ! child )
                        return false;
                    if ( ! TraverseNode( child, visitor ) )
                        return false;
                }
            }

            return true;
        }
    };

    class FbxMeshLoader
    {
    public:
        ImportedScene LoadSceneToMesh( FbxScene& scene )
        {
            ImportedScene res;

            int ntextures = scene.GetTextureCount();
            int nmaterials = scene.GetMaterialCount();
            res.textures.reserve( ntextures );
            res.materials.reserve( nmaterials );
            for ( int i = 0; i < ntextures; ++i )
            {
                FbxFileTexture* tex = FbxCast<FbxFileTexture>( scene.GetTexture( i ) );
                tex->SetUserDataPtr( (void*)i );
                res.textures.emplace_back( tex->GetFileName(), TextureID::nullid );
            }

            for ( int i = 0; i < nmaterials; ++i )
            {
                FbxSurfaceMaterial* material = scene.GetMaterial( i );
                material->SetUserDataPtr( (void*)i );

                std::string material_name = material->GetName();
                const FbxProperty albedo = material->FindProperty( FbxSurfaceMaterial::sDiffuse );
                if ( !albedo.IsValid() )
                    throw SnowEngineException( "no albedo" );

                auto* albedo_texture = albedo.GetSrcObject<FbxFileTexture>();
                if ( !albedo_texture )
                    throw SnowEngineException( "no texture for albedo" );

                const FbxProperty normal = material->FindProperty( FbxSurfaceMaterial::sNormalMap );
                if ( !normal.IsValid() )
                    throw SnowEngineException( "no normal map" );

                auto* normal_texture = normal.GetSrcObject<FbxFileTexture>();
                if ( !normal_texture )
                    throw SnowEngineException( "no texture for normal map" );

                int spec_idx = -1;
                const FbxProperty specular = material->FindProperty( FbxSurfaceMaterial::sSpecular );
                if ( specular.IsValid() )
                {
                    auto* specular_texture = specular.GetSrcObject<FbxFileTexture>();
                    if ( specular_texture )
                        spec_idx = reinterpret_cast<int>( specular_texture->GetUserDataPtr() );
                }

                res.materials.emplace_back( material_name,
                                            ImportedScene::SceneMaterial{ reinterpret_cast<int>( albedo_texture->GetUserDataPtr() ),
                                                                       reinterpret_cast<int>( normal_texture->GetUserDataPtr() ),
                                                                       spec_idx } );
            }

            FbxSceneTraverser traverser;
            FbxPrepass prepass( res );
            if ( ! traverser.TraverseFbxScene( scene, prepass ) )
                throw SnowEngineException( "fail in TraverseFbxScene" );

            return res;
        }

    private:
        struct FbxSubmesh
        {
            const FbxMesh* mesh = nullptr;
            int mesh_idx = -1;
            int material_idx = -1;
            int triangle_count = 0;
            int index_offset = 0;
        };
        struct FbxRenderitem
        {
            FbxNode* node = nullptr;
            int material_idx = -1;
        };
        struct PrepassData
        {
            using SubmeshKey = std::pair<const FbxMesh*, int>;

            std::unordered_map<SubmeshKey, FbxSubmesh, boost::hash<SubmeshKey>> submeshes;
            std::unordered_map<const FbxMesh*, std::unordered_set<int>> mesh2submeshes;

            std::unordered_map<fbxsdk::FbxUInt64, int> mesh_id2index;
            int num_renderitems = 0;
            int triangle_count_total = 0;
            int num_meshes = 0;
        };

        class FbxPrepass: public FbxSceneVisitor
        {
        public:
            FbxPrepass(ImportedScene& scene)
                : m_scene( scene )
            {
            }
            // Inherited via FbxSceneVisitor
            virtual bool VisitNode( FbxNode* node, bool& visit_children ) override
            {
                FbxNodeAttribute* main_attrib = node->GetNodeAttribute();
                if ( ! main_attrib || main_attrib->GetAttributeType() != FbxNodeAttribute::eMesh )
                    return true;

                FbxMesh* mesh = node->GetMesh();
                const fbxsdk::FbxUInt64 mesh_id = mesh->GetUniqueID();
                int mesh_idx = -1;
                if ( auto mesh2idx = m_data.mesh_id2index.find(mesh_id); mesh2idx == m_data.mesh_id2index.end() )
                {
                    mesh_idx = int( m_scene.meshes.size() );
                    m_data.mesh_id2index[mesh_id] = mesh_idx;

                    auto& new_mesh = m_scene.meshes.emplace_back();
                    LoadMesh( mesh, new_mesh );
                    mesh2idx = m_data.mesh_id2index.find(mesh_id);
                }
                else
                {
                    mesh_idx = mesh2idx->second;
                }

                if (mesh_idx < 0)
                    throw SnowEngineException( "[Error][SceneImporter]: failed to load mesh for the node" );

                auto& new_node = m_scene.nodes.emplace_back();
                new_node.mesh_idx = mesh_idx;
                const auto& transform = node->EvaluateGlobalTransform();
                DirectX::XMFLOAT4X4 dxtf( transform.Get( 0, 0 ), transform.Get( 0, 1 ), transform.Get( 0, 2 ), transform.Get( 0, 3 ),
                                          transform.Get( 1, 0 ), transform.Get( 1, 1 ), transform.Get( 1, 2 ), transform.Get( 1, 3 ), 
                                          transform.Get( 2, 0 ), transform.Get( 2, 1 ), transform.Get( 2, 2 ), transform.Get( 2, 3 ), 
                                          transform.Get( 3, 0 ), transform.Get( 3, 1 ), transform.Get( 3, 2 ), transform.Get( 3, 3 ) );
                new_node.transform = dxtf;

                return true;
            }

        private:

            void LoadMesh( FbxMesh* fbx_mesh, ImportedScene::Mesh& out_mesh )
            {
                assert( fbx_mesh );
                out_mesh.name = fbx_mesh->GetName();

                const int mesh_polygon_count = fbx_mesh->GetPolygonCount();
                out_mesh.indices.reserve( mesh_polygon_count );

                FbxLayerElementArrayTemplate<int>* materials = nullptr;
                FbxGeometryElement::EMappingMode material_mapping_mode = FbxGeometryElement::eNone;

                auto* mesh_element_material = fbx_mesh->GetElementMaterial();
                if ( mesh_element_material )
                {
                    materials = &mesh_element_material->GetIndexArray();
                    material_mapping_mode = mesh_element_material->GetMappingMode();
                    if ( material_mapping_mode == FbxGeometryElement::eByPolygon )
                    {
                        if ( materials->GetCount() != mesh_polygon_count )
                            throw SnowEngineException( "per-polygon material mapping is invalid" );

                        std::unordered_map<int/*material_idx*/, uint32_t/*submesh_idx*/> mat2submesh;

                        for ( int polygon_idx = 0; polygon_idx < mesh_polygon_count; ++polygon_idx )
                        {
                            int material_idx = materials->GetAt( polygon_idx );
                            material_idx = reinterpret_cast<int>( fbx_mesh->GetNode()->GetMaterial( material_idx )->GetUserDataPtr() );

                            uint32_t submesh_idx = -1;
                            if ( auto mat_submesh_idx_it = mat2submesh.find( material_idx ); mat_submesh_idx_it == mat2submesh.end() )
                            {
                                submesh_idx = out_mesh.submeshes.size();
                                mat2submesh[material_idx] = submesh_idx;
                                auto& new_submesh = out_mesh.submeshes.emplace_back();
                                new_submesh.material_idx = material_idx;
                                new_submesh.name = out_mesh.name + "_" + m_scene.materials[material_idx].first;
                                new_submesh.index_offset = 0;
                                new_submesh.nindices = 0;
                            }
                            else
                            {
                                submesh_idx = mat_submesh_idx_it->second;
                            }

                            auto& submesh = out_mesh.submeshes[submesh_idx];
                            submesh.nindices += 3;
                        }

                        uint32_t nindices = 0;
                        for ( auto& submesh : out_mesh.submeshes )
                        {
                            submesh.index_offset = nindices;
                            nindices += submesh.nindices;
                            submesh.nindices = 0;
                        }

                        FbxStringList uv_names;
                        fbx_mesh->GetUVSetNames( uv_names );
                        const char* uv_name = uv_names[0];

                        const FbxVector4* ctrl_pts = fbx_mesh->GetControlPoints();
                        out_mesh.vertices.resize( nindices );
                        out_mesh.indices.resize( nindices );
                        for ( uint32_t i = 0; i < nindices; ++i )
                            out_mesh.indices[i] = i;

                        for ( int polygon_idx = 0; polygon_idx < mesh_polygon_count; ++polygon_idx )
                        {
                            int material_idx = materials->GetAt( polygon_idx );
                            material_idx = reinterpret_cast<int>( fbx_mesh->GetNode()->GetMaterial( material_idx )->GetUserDataPtr() );

                            auto& submesh = out_mesh.submeshes[mat2submesh[material_idx]];

                            int out_triangle_indices[3] = { -1,-1,-1 };
                            for ( int fb_idx_in_poly = 0; fb_idx_in_poly < 3; ++fb_idx_in_poly )
                            {
                                int cp_idx = fbx_mesh->GetPolygonVertex( polygon_idx, fb_idx_in_poly );
                                int out_idx = int( submesh.index_offset + submesh.nindices ) + 2 - fb_idx_in_poly;
                                out_triangle_indices[fb_idx_in_poly] = out_idx;

								auto& out_vertex = out_mesh.vertices[out_idx];
                                auto& vpos = out_vertex.pos;
                                vpos.x = -ctrl_pts[cp_idx][0];
                                vpos.y = ctrl_pts[cp_idx][1];
                                vpos.z = ctrl_pts[cp_idx][2];

                                FbxVector4 normal;
                                if ( ! fbx_mesh->GetPolygonVertexNormal( polygon_idx, fb_idx_in_poly, normal ) )
                                    throw SnowEngineException( "failed to extract normal" );
                                auto& vnormal = out_mesh.vertices[out_idx].normal;
                                vnormal.x = -normal[0];
                                vnormal.y = normal[1];
                                vnormal.z = normal[2];

                                FbxVector2 uv;
                                bool unused;
                                if ( ! fbx_mesh->GetPolygonVertexUV( polygon_idx, fb_idx_in_poly, uv_name, uv, unused ) )
                                    throw SnowEngineException( "failed to extract uv" );

                                out_vertex.uv.x = uv[0];
                                out_vertex.uv.y = 1.0f - uv[1];
                            }

                            DirectX::XMFLOAT3 dp1 = out_mesh.vertices[out_triangle_indices[1]].pos - out_mesh.vertices[out_triangle_indices[0]].pos;
                            DirectX::XMFLOAT3 dp2 = out_mesh.vertices[out_triangle_indices[2]].pos - out_mesh.vertices[out_triangle_indices[0]].pos;

                            DirectX::XMFLOAT2 duv1 = out_mesh.vertices[out_triangle_indices[1]].uv - out_mesh.vertices[out_triangle_indices[0]].uv;
                            DirectX::XMFLOAT2 duv2 = out_mesh.vertices[out_triangle_indices[2]].uv - out_mesh.vertices[out_triangle_indices[0]].uv;

                            float inv_det = 1.0f / ( duv1.x * duv2.y - duv1.y * duv2.x );
                            DirectX::XMFLOAT3 triangle_tangent = ( inv_det * ( dp1 * duv2.y - dp2 * duv1.y ) );
                            XMFloat3Normalize( triangle_tangent );
                            for (int i = 0; i < 3; ++i )
                                out_mesh.vertices[out_triangle_indices[i]].tangent = triangle_tangent;

                            submesh.nindices += 3;							
                        }

						static int k = 0;
						if (!k)
							DeduplicateVertices( out_mesh.vertices, out_mesh.indices );
						k++;
                    }
                }
            }

			bool DeduplicateVertices( std::vector<Vertex>& vertices, std::vector<uint32_t>& indices )
			{
				if ( vertices.empty() )
					return true;

				std::vector<uint32_t> replacements;
				replacements.resize(vertices.size());
				for ( uint32_t i = 0; i < replacements.size(); ++i )
					replacements[i] = i;


				// terrible O(n^2) deduplication. improve this if you have some time
				uint32_t unique_vertices = 1;
				for ( uint32_t i = 1; i < vertices.size(); ++i )
				{
					uint32_t j = 0;
					for ( ; j < unique_vertices; ++j )
					{
						const auto& rhs = vertices[i];
						const auto& lhs = vertices[j];
						if ( lhs.pos == rhs.pos
							&& lhs.normal == rhs.normal
							&& lhs.tangent == rhs.tangent
							&& lhs.uv == rhs.uv)
						{
							break;
						}
					}
					replacements[i] = j;

					if ( j == unique_vertices )
						vertices[unique_vertices++] = vertices[i];
				}

				vertices.resize(unique_vertices);

				for ( auto& idx: indices )
					idx = replacements[idx];

				return true;
			}

            PrepassData m_data;
            ImportedScene& m_scene;
        };

        

    };

    class FbxScenePrinter
    {
    public:
        FbxScenePrinter() = default;
        bool PrintScene( const FbxScene& scene, std::ostream& out )
        {
            FbxNode* root_node = scene.GetRootNode();
            if ( ! root_node )
                return false;

            out << "RootNode\n";
            m_ntabs++;

            for ( int i = 0; i < root_node->GetChildCount(); ++i )
            {
                PrintTabs( out );
                PrintNode( *root_node->GetChild( i ), out );
            }

            m_ntabs--;
            return true;
        }
    private:

        void PrintTabs( std::ostream& out ) const
        {
            for ( int i = 0; i < m_ntabs; ++i )
                out << '\t';
        }

        FbxString GetAttributeTypeName( FbxNodeAttribute::EType type ) const
        {
            switch ( type ) {
                case FbxNodeAttribute::eUnknown: return "unidentified";
                case FbxNodeAttribute::eNull: return "null";
                case FbxNodeAttribute::eMarker: return "marker";
                case FbxNodeAttribute::eSkeleton: return "skeleton";
                case FbxNodeAttribute::eMesh: return "mesh";
                case FbxNodeAttribute::eNurbs: return "nurbs";
                case FbxNodeAttribute::ePatch: return "patch";
                case FbxNodeAttribute::eCamera: return "camera";
                case FbxNodeAttribute::eCameraStereo: return "stereo";
                case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
                case FbxNodeAttribute::eLight: return "light";
                case FbxNodeAttribute::eOpticalReference: return "optical reference";
                case FbxNodeAttribute::eOpticalMarker: return "optical marker";
                case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
                case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
                case FbxNodeAttribute::eBoundary: return "boundary";
                case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
                case FbxNodeAttribute::eShape: return "shape";
                case FbxNodeAttribute::eLODGroup: return "lodgroup";
                case FbxNodeAttribute::eSubDiv: return "subdiv";
                default: return "unknown";
            }
        }

        void PrintAttribute( const FbxNodeAttribute& attrib, std::ostream& out ) const
        {
            FbxString type_name = GetAttributeTypeName( attrib.GetAttributeType() );
            FbxString attr_name = attrib.GetName();

            out << type_name.Buffer() << ' ' << attr_name.Buffer() << '\n';
        }

        void PrintNode( FbxNode& node, std::ostream& out )
        {
            const char* node_name = node.GetName();
            FbxDouble3 translation = node.LclTranslation.Get();
            FbxDouble3 rotation = node.LclRotation.Get();
            FbxDouble3 scaling = node.LclScaling.Get();

            out << node_name << ' ' << translation[0] << ' ' << rotation[0] << ' ' << scaling[0] << '\n';
            PrintTabs( out );
            out << "Attributes:\n";
            m_ntabs++;
            for ( int i = 0; i < node.GetNodeAttributeCount(); ++i )
            {
                if ( node.GetNodeAttributeByIndex( i )->GetAttributeType() == FbxNodeAttribute::eMesh )
                {
                    const auto* pMesh = node.GetMesh();

                    const int lPolygonCount = pMesh->GetPolygonCount();

                    // Count the polygon count of each material
                    FbxLayerElementArrayTemplate<int>* lMaterialIndice = NULL;
                    FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
                    if ( pMesh->GetElementMaterial() )
                    {
                        lMaterialIndice = &pMesh->GetElementMaterial()->GetIndexArray();
                        lMaterialMappingMode = pMesh->GetElementMaterial()->GetMappingMode();
                        if ( lMaterialMappingMode == FbxGeometryElement::eByPolygon )
                        {
                            int multimaterial = 1;
                        }
                    }
                    else
                    {
                        int cop = 1;
                    }
                }

                PrintTabs( out );
                PrintAttribute( *node.GetNodeAttributeByIndex( i ), out );
            }
            m_ntabs--;
            out << '\n';
            PrintTabs( out );
            out << "Children:\n";
            m_ntabs++;
            for ( int i = 0; i < node.GetChildCount(); ++i )
            {
                PrintTabs( out );
                PrintNode( *node.GetChild( i ), out );
            }
            m_ntabs--;
            out << '\n';
        }

        int m_ntabs = 0;
    };
}
bool LoadFbxFromFile( const std::string& filename, ImportedScene& mesh )
{
    std::cout << "Loading " << filename << std::endl;

    FbxManager* fbx_mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create( fbx_mgr, IOSROOT );
    fbx_mgr->SetIOSettings( ios );

    FbxImporter* importer = FbxImporter::Create( fbx_mgr, "" );

    if ( ! importer->Initialize( filename.c_str(), -1, fbx_mgr->GetIOSettings() ) )
    {
        std::cerr << "FbxManager::Initialize failed\n" << '\t' << importer->GetStatus().GetErrorString();
        return false;
    }

    FbxScene* scene = FbxScene::Create( fbx_mgr, std::filesystem::path( filename ).filename().generic_string().c_str() );

    importer->Import( scene );
    importer->Destroy();

    FbxMeshLoader mesh_loader;
    mesh = mesh_loader.LoadSceneToMesh( *scene );

    fbx_mgr->Destroy();
    return true;
}
