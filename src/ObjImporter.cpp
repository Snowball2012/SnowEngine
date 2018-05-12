#include "stdafx.h"

#include "ObjImporter.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <fbxsdk.h>

bool LoadObjFromFile( const std::string& filename, StaticMesh& mesh )
{
	std::cout << "Loading " << filename << std::endl;

	boost::filesystem::path dir = filename;
	dir.remove_filename();
	dir += boost::filesystem::path::preferred_separator;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	const bool ret = tinyobj::LoadObj( &attrib, &shapes, &materials, &err, filename.c_str(),
	                                   dir.string().c_str(), true );

	if ( ! ret )
		return false;

	mesh.submeshes.clear();
	mesh.vertices.clear();
	//if ( attrib.vertices.size() != attrib.normals.size() ||
	//	 attrib.texcoords.size() / 2 != attrib.vertices.size() / 3 )
	//	return false;

	const size_t nvertices = attrib.vertices.size() / 3;
	mesh.vertices.reserve( nvertices );
	for ( size_t i = 0; i < nvertices; ++i )
	{
		const float* v = attrib.vertices.data() + i * 3;
		//const float* n = attrib.normals.data() + i * 3;
		//const float* uv = attrib.texcoords.data() + i * 2;
		mesh.vertices.push_back( {
			DirectX::XMFLOAT3( v )/*,
			DirectX::XMFLOAT3( n ),
			DirectX::XMFLOAT2( uv )*/
		} );
	}

	size_t indices_total = 0;
	for ( const auto& shape : shapes )
	{
		mesh.submeshes.push_back( { shape.name, shape.mesh.indices.size(), indices_total, -1 } );
		for ( const auto& mat_idx : shape.mesh.material_ids )
			if ( mat_idx != -1 )
			{
				mesh.submeshes.back().material_idx = mat_idx;
				break;
			}

		indices_total += shape.mesh.indices.size();
	}

	mesh.indices.reserve( indices_total );
	for ( const auto& shape : shapes )
		for ( const auto& idx : shape.mesh.indices )
		{
			mesh.indices.push_back( idx.vertex_index );
			mesh.vertices[idx.vertex_index].normal = DirectX::XMFLOAT3( attrib.normals.data() + idx.normal_index * 3 );
			mesh.vertices[idx.vertex_index].uv = DirectX::XMFLOAT2( attrib.texcoords.data() + idx.texcoord_index * 2 );
		}

	mesh.materials.reserve( materials.size() );
	for ( const auto& material : materials )
	{
		boost::filesystem::path path( dir );
		path /= material.name;
		path.remove_leaf();
		path /= material.normal_texname;
		mesh.materials.emplace_back( path.string(), StaticMesh::SceneMaterial{ int( mesh.textures.size() ), -1 } );
		mesh.textures.emplace_back( path.string() );
	}
	
	return ret;
}


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
		bool TraverseFbxScene( FbxScene& scene, FbxSceneVisitor& visitor )
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

	// very simple inefficient solution. Duplicate vertex attributes for each triangle 
	class FbxMeshLoader
	{
	public:
		StaticMesh LoadSceneToMesh( FbxScene& scene )
		{
			StaticMesh res;

			/*FbxAxisSystem axis_system( FbxAxisSystem::EUpVector::eYAxis, FbxAxisSystem::EFrontVector::eParityOdd, FbxAxisSystem::ECoordSystem::eLeftHanded );

			axis_system.ConvertScene( &scene );*/

			int ntextures = scene.GetTextureCount();
			int nmaterials = scene.GetMaterialCount();
			res.textures.reserve( ntextures );
			res.materials.reserve( nmaterials );
			for ( int i = 0; i < ntextures; ++i )
			{
				FbxFileTexture* tex = FbxCast<FbxFileTexture>( scene.GetTexture( i ) );
				tex->SetUserDataPtr( (void*)i );
				{
					double ru = tex->GetRotationU();
					double rv = tex->GetRotationV();
					double su = tex->GetScaleU();
					double sv = tex->GetScaleV();
					double tu = tex->GetTranslationU();
					double tv = tex->GetTranslationV();
					double wrk = 0;
				}
				res.textures.emplace_back( tex->GetFileName() );
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
											StaticMesh::SceneMaterial{ reinterpret_cast<int>( albedo_texture->GetUserDataPtr() ),
											                           reinterpret_cast<int>( normal_texture->GetUserDataPtr() ),
											                           spec_idx } );
			}

			FbxSceneTraverser traverser;
			FbxPrepass prepass;
			if ( ! traverser.TraverseFbxScene( scene, prepass ) )
				throw SnowEngineException( "fail in TraverseFbxScene" );

			PrepassData prepass_data = prepass.MoveData();

			int cur_idx_offset = 0;

			for ( auto& submesh : prepass_data.submeshes )
			{
				submesh.second.index_offset = cur_idx_offset;
				cur_idx_offset += submesh.second.triangle_count * 3;
			}

			res.indices.resize( cur_idx_offset );
			for ( int i = 0; i < cur_idx_offset; ++i )
				res.indices[i] = i;

			res.submeshes.reserve( prepass_data.num_renderitems );

			if ( ! traverser.TraverseFbxScene( scene, FbxRenderitemPass( prepass_data, res ) ) )
				throw SnowEngineException( "fail in TraverseFbxScene" );
				
			res.vertices.resize( cur_idx_offset );

			for ( auto& submesh : prepass_data.submeshes )
				submesh.second.triangle_count = 0;

			for ( auto& mesh : prepass_data.mesh2submeshes )
			{
				const FbxMesh* fbmesh = mesh.first;

				FbxStringList uv_names;
				fbmesh->GetUVSetNames( uv_names );
				const char* uv_name = uv_names[0];

				const FbxVector4* ctrl_pts = fbmesh->GetControlPoints();

				FbxLayerElementArrayTemplate<int>* materials = nullptr;

				auto* mesh_element_material = fbmesh->GetElementMaterial();
				materials = &mesh_element_material->GetIndexArray();

				for ( int fb_polygon_idx = 0; fb_polygon_idx < fbmesh->GetPolygonCount(); ++fb_polygon_idx )
				{
					int material_idx = materials->GetAt( fb_polygon_idx );
					material_idx = reinterpret_cast<int>( fbmesh->GetNode()->GetMaterial( material_idx )->GetUserDataPtr() );

					FbxSubmesh& submesh = prepass_data.submeshes.find( std::make_pair( fbmesh, material_idx ) )->second;

					for ( int fb_idx_in_poly = 0; fb_idx_in_poly < 3; ++fb_idx_in_poly )
					{
						int cp_idx = fbmesh->GetPolygonVertex( fb_polygon_idx, fb_idx_in_poly );
						int out_idx = submesh.index_offset + submesh.triangle_count * 3 + (2 - fb_idx_in_poly);

						res.vertices[out_idx].pos.x = -ctrl_pts[cp_idx][0];
						res.vertices[out_idx].pos.y = ctrl_pts[cp_idx][1];
						res.vertices[out_idx].pos.z = ctrl_pts[cp_idx][2];

						FbxVector4 normal;
						if ( ! fbmesh->GetPolygonVertexNormal( fb_polygon_idx, fb_idx_in_poly, normal ) )
							throw SnowEngineException( "failed to extract normal" );
						res.vertices[out_idx].normal.x = -normal[0];
						res.vertices[out_idx].normal.y = normal[1];
						res.vertices[out_idx].normal.z = normal[2];

						FbxVector2 uv;
						bool unused;
						if ( ! fbmesh->GetPolygonVertexUV( fb_polygon_idx, fb_idx_in_poly, uv_name, uv, unused ) )
							throw SnowEngineException( "failed to extract uv" );

						res.vertices[out_idx].uv.x = uv[0];
						res.vertices[out_idx].uv.y = 1.0f - uv[1];
					}
					submesh.triangle_count++;
				}
			}


			return res;
		}

	private:
		struct FbxSubmesh
		{
			const FbxMesh* mesh = nullptr;
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
			int num_renderitems = 0;
			int triangle_count_total = 0;
		};

		class FbxPrepass: public FbxSceneVisitor
		{
		public:
			// Inherited via FbxSceneVisitor
			virtual bool VisitNode( FbxNode* node, bool& visit_children ) override
			{
				FbxNodeAttribute* main_attrib = node->GetNodeAttribute();
				if ( ! main_attrib || main_attrib->GetAttributeType() != FbxNodeAttribute::eMesh )
					return true;

				FbxMesh* mesh = node->GetMesh();
				if ( m_data.mesh2submeshes.count( mesh ) == 0 )
					VisitMesh( mesh );

				m_data.num_renderitems += m_data.mesh2submeshes[mesh].size();

				return true;
			}

			PrepassData MoveData()
			{
				return std::move( m_data );
			}
		private:
			void VisitMesh( FbxMesh* mesh )
			{
				const int mesh_polygon_count = mesh->GetPolygonCount();

				// Count the polygon count of each material
				FbxLayerElementArrayTemplate<int>* materials = nullptr;
				FbxGeometryElement::EMappingMode material_mapping_mode = FbxGeometryElement::eNone;

				const int n_submeshes = m_data.submeshes.size();

				auto* mesh_element_material = mesh->GetElementMaterial();
				if ( mesh_element_material )
				{
					materials = &mesh_element_material->GetIndexArray();
					material_mapping_mode = mesh_element_material->GetMappingMode();
					if ( materials && material_mapping_mode == FbxGeometryElement::eByPolygon )
					{
						if ( materials->GetCount() != mesh_polygon_count )
							throw SnowEngineException( "per-polygon material mapping is invalid" );

						for ( int polygon_idx = 0; polygon_idx < mesh_polygon_count; ++polygon_idx )
						{
							int material_idx = materials->GetAt( polygon_idx );
							material_idx = reinterpret_cast<int>( mesh->GetNode()->GetMaterial( material_idx )->GetUserDataPtr() );
							auto& submesh = m_data.submeshes[std::make_pair( mesh, material_idx )];
							submesh.material_idx = material_idx;
							submesh.mesh = mesh;
							submesh.triangle_count++;
							m_data.mesh2submeshes[mesh].insert( material_idx );
						}
					}
				}
				m_data.triangle_count_total += mesh_polygon_count;
			}

			PrepassData m_data;			
		};

		class FbxRenderitemPass: public FbxSceneVisitor
		{
		public:
			FbxRenderitemPass( const PrepassData& prepass_data, StaticMesh& mesh )
				: m_mesh( mesh ), m_data( prepass_data )
			{}

			// Inherited via FbxSceneVisitor
			virtual bool VisitNode( FbxNode* node, bool& visit_children ) override
			{
				FbxNodeAttribute* main_attrib = node->GetNodeAttribute();
				if ( ! main_attrib || main_attrib->GetAttributeType() != FbxNodeAttribute::eMesh )
					return true;
				
				std::string name = node->GetName();
				name += node->GetMesh()->GetName();
				
				const auto& transform = node->EvaluateGlobalTransform();
				DirectX::XMFLOAT4X4 dxtf( transform.Get( 0, 0 ), transform.Get( 0, 1 ), transform.Get( 0, 2 ), transform.Get( 0, 3 ),
										  transform.Get( 1, 0 ), transform.Get( 1, 1 ), transform.Get( 1, 2 ), transform.Get( 1, 3 ), 
										  transform.Get( 2, 0 ), transform.Get( 2, 1 ), transform.Get( 2, 2 ), transform.Get( 2, 3 ), 
										  transform.Get( 3, 0 ), transform.Get( 3, 1 ), transform.Get( 3, 2 ), transform.Get( 3, 3 ) );

				for ( int material_idx : m_data.mesh2submeshes.find( node->GetMesh() )->second )
				{
					const auto& submesh = m_data.submeshes.find( std::make_pair( node->GetMesh(), material_idx ) )->second;
					name += std::to_string( material_idx );
					m_mesh.submeshes.push_back( { name, size_t( submesh.triangle_count * 3 ), size_t( submesh.index_offset ), material_idx, dxtf } );
				}

				return true;
			}

		private:

			const PrepassData& m_data;
			StaticMesh& m_mesh;
		};

	};

	class FbxScenePrinter
	{
	public:
		FbxScenePrinter() = default;
		bool PrintScene( FbxScene& scene, std::ostream& out )
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
				case FbxNodeAttribute::eOpticalMarker: return "marker";
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
						if ( lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon )
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
bool LoadFbxFromFile( const std::string& filename, StaticMesh& mesh )
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

	FbxScene* scene = FbxScene::Create( fbx_mgr, boost::filesystem::path( filename ).leaf().string().c_str() );

	importer->Import( scene );
	importer->Destroy();

	FbxMeshLoader mesh_loader;
	mesh = mesh_loader.LoadSceneToMesh( *scene );

	fbx_mgr->Destroy();
	return true;
}
