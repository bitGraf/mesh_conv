#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>

#include <laml/laml.hpp>

#include <assimp/Importer.hpp>

#include "skeleton.h"
#include "animation.h"

namespace rh {

	struct Vertex {
		laml::Vec3 position;
		laml::Vec3 normal;
		laml::Vec3 tangent;
		laml::Vec3 bitangent;
		laml::Vec2 tex;

		// animated meshes only
		std::vector<u32> bone_indices;
		std::vector<f32> bone_weights;
	};

	struct SubMesh {
		u32 start_vertex;
		u32 start_index;
		u32 mat_index;
		u32 num_indices;
		u32 num_vertices;

		laml::Mat4 local_matrix;
		laml::Mat4 transform;

		std::string node_name, mesh_name;
		bool rigged = false;
	};

	struct MeshData {
		u32 num_verts;
		u32 num_inds;
		u32 num_submeshes;
		bool has_skeleton;

		std::vector<SubMesh> submeshes;
		std::vector<Vertex> vertices;
		std::vector<u32> indices;
		Skeleton bind_pose;
	};

	bool extract_meshes(const aiScene* scene, MeshData& mesh);
}