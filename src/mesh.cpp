#include "mesh.h"

#include <set>

#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/postprocess.h>

#include "animation.h"
#include "utils.h"

namespace rh {

	void normalize_weights(Vertex& vert) {
		if (vert.bone_indices.size() < 4) {
			for (u32 n = vert.bone_indices.size(); n < 4; n++) {
				vert.bone_indices.push_back(0);
				vert.bone_weights.push_back(0);
			}
			return;
		}

		if (vert.bone_indices.size() > 4) {
			printf("More than 4 weights!\n");
			// sort by 4 largest weights
			for (u32 i = 0; i < 4; i++) {
				for (u32 j = i; j < vert.bone_weights.size(); j++) {
					if (vert.bone_weights[i] > vert.bone_weights[j]) {
						// swap
						f32 tmp1 = vert.bone_weights[j];
						vert.bone_weights[j] = vert.bone_weights[i];
						vert.bone_weights[i] = tmp1;

						u32 tmp2 = vert.bone_indices[j];
						vert.bone_indices[j] = vert.bone_indices[i];
						vert.bone_indices[i] = tmp2;
					}
				}
			}

			f32 total = 0.0f;
			// normalize weights to sum to 1
			for (u32 n = 0; n < 4; n++) {
				total += vert.bone_weights[n];
			}
			for (u32 n = 0; n < 4; n++) {
				vert.bone_weights[n] = vert.bone_weights[n] / total;
			}
		}
	}

	void traverse_node(const aiNode* node, const laml::Mat4& parent_transform, std::unordered_map<const aiNode*, std::vector<u32>>& node_map, std::vector<SubMesh>& submeshes) {
		laml::Mat4 local_transform = utils::mat4_from_aiMatrix4x4(node->mTransformation);
		laml::Mat4 transform = laml::mul(parent_transform, local_transform);

		node_map[node].resize(node->mNumMeshes);
		for (u32 n = 0; n < node->mNumMeshes; n++) {
			u32 mesh_id = node->mMeshes[n];
			auto& submesh = submeshes[mesh_id];
			submesh.node_name = node->mName.C_Str();
			submesh.transform = transform;
			submesh.local_matrix = local_transform;
			std::cout << "transform: " << local_transform << std::endl;
			node_map[node][n] = mesh_id;
		}

		for (u32 n = 0; n < node->mNumChildren; n++) {
			traverse_node(node->mChildren[n], transform, node_map, submeshes);
		}
	}

	bool extract_meshes(const aiScene* scene, MeshData& mesh) {
		u32 vertex_count = 0;
		u32 index_count = 0;
		u32 submesh_count = 0;

		mesh.submeshes.reserve(scene->mNumMeshes);
		for (u32 m = 0; m < scene->mNumMeshes; m++) {
			const aiMesh* s_mesh = scene->mMeshes[m];

			SubMesh sm;
			sm.start_index = index_count;
			sm.start_vertex = vertex_count;
			sm.mat_index = s_mesh->mMaterialIndex;
			sm.num_vertices = s_mesh->mNumVertices;
			sm.num_indices = s_mesh->mNumFaces * 3;
			sm.mesh_name = s_mesh->mName.C_Str();
			sm.local_matrix = laml::Mat4(1.0f);
			sm.transform = laml::Mat4(1.0f);
			mesh.submeshes.push_back(sm);

			vertex_count += sm.num_vertices;
			index_count += sm.num_indices;
			submesh_count++;

			assert(s_mesh->HasPositions() && "Meshes require positions!");
			assert(s_mesh->HasNormals() && "Meshes require normals!");
			assert(s_mesh->HasTangentsAndBitangents() && "Meshes require tangents and bitangents!");
			assert(s_mesh->HasTextureCoords(0) && "Meshes require texture coords!");

			// vertices
			for (u32 i = 0; i < sm.num_vertices; i++) {
				Vertex vert;

				vert.position = utils::vec3_from_aiVector3D(s_mesh->mVertices[i]);
				vert.normal = utils::vec3_from_aiVector3D(s_mesh->mNormals[i]);
				vert.tangent = utils::vec3_from_aiVector3D(s_mesh->mTangents[i]);
				vert.bitangent = utils::vec3_from_aiVector3D(s_mesh->mBitangents[i]);
				vert.tex = utils::vec2_from_aiVector3D(s_mesh->mTextureCoords[0][i]);

				std::cout << "vert: " << vert.position << std::endl;

				mesh.vertices.push_back(vert);
			}

			// indices
			for (u32 i = 0; i < s_mesh->mNumFaces; i++) {
				assert(s_mesh->mFaces[i].mNumIndices == 3 && "Faces must have 3 indices!");
				const auto& face = s_mesh->mFaces[i];

				mesh.indices.push_back(face.mIndices[0]);
				mesh.indices.push_back(face.mIndices[1]);
				mesh.indices.push_back(face.mIndices[2]);
			}
		}

		mesh.num_inds = index_count;
		mesh.num_verts = vertex_count;
		mesh.num_submeshes = submesh_count;

		std::unordered_map<const aiNode*, std::vector<u32>> node_map;
		traverse_node(scene->mRootNode, laml::Mat4(1.0f), node_map, mesh.submeshes);

		// get bone weights
		if (mesh.has_skeleton) {
			for (u32 m = 0; m < scene->mNumMeshes; m++) {
				const aiMesh* s_mesh = scene->mMeshes[m];
				SubMesh& sm = mesh.submeshes[m];

				if (s_mesh->mNumBones > 0) {
					sm.rigged = true;

					for (u32 i = 0; i < s_mesh->mNumBones; i++) {
						const aiBone* bone = s_mesh->mBones[i];

						bool hasNonZeroWeight = false;
						for (u32 j = 0; j < bone->mNumWeights; j++) {
							if (bone->mWeights[j].mWeight > 0.000001f) {
								hasNonZeroWeight = true;
								break;
							}
						}
						if (!hasNonZeroWeight)
							continue;

						// find bone in skeleton
						u32 bone_index = get_bone_index(mesh.bind_pose, bone->mName.C_Str());
						if (bone_index == Skeleton::NullIndex) {
							printf("Could not find mesh bone '%s' in skeleton!\n", bone->mName.C_Str());
						}

						//u32 bone_info_index = ~0;
						//for (u32 j = 0; j < mesh.bind_pose.bones.size(); j++) {
						//	
						//}

						for (u32 j = 0; j < bone->mNumWeights; j++) {
							s32 vert_id = sm.start_vertex + bone->mWeights[j].mVertexId;
							f32 weight = bone->mWeights[j].mWeight;

							mesh.vertices[vert_id].bone_indices.push_back(bone_index);
							mesh.vertices[vert_id].bone_weights.push_back(weight);
						}
					}
				}
			}

			for (auto& vert : mesh.vertices) {
				normalize_weights(vert);
			}
		}

		return true;
	}
	
}