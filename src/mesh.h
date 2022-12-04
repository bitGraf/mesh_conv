#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>

#include <laml/laml.hpp>

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
		laml::Vector<s32, 4> bone_indices; // glsl ivec4
        laml::Vector<f32, 4> bone_weights; // just a Vec4
	};

	struct SubMesh {
		u32 start_vertex;
		u32 start_index;
		u32 mat_index;
		u32 num_indices;
		u32 num_vertices;

		laml::Mat4 local_matrix;
		laml::Mat4 model_matrix;
	};

	struct MeshData {
		u32 num_verts;
		u32 num_inds;
		u32 num_submeshes;
		bool has_skeleton; // also means its rigged to that skeleton

		std::vector<SubMesh> submeshes;
		std::vector<Vertex> vertices;
		std::vector<u32> indices;
		Skeleton bind_pose;

        std::string mesh_name;
	};
}