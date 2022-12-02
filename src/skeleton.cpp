#include "skeleton.h"
#include "utils.h"

#include <set>

namespace rh {

	void traverse_bone(const aiNode* node, Skeleton& skeleton, u32 parent_idx) {
		u32 bone_index = static_cast<u32>(skeleton.bones.size());
		bone_info new_bone;
		new_bone.name = std::string(node->mName.C_Str());
		new_bone.parent_idx = parent_idx;
		new_bone.local_matrix = utils::mat4_from_aiMatrix4x4(node->mTransformation);
		skeleton.bones.push_back(new_bone);

		for (u32 node_index = 0; node_index < node->mNumChildren; node_index++) {
			traverse_bone(node->mChildren[node_index], skeleton, bone_index);
		}
	}

	void traverse_node(const aiNode* node, Skeleton& skeleton) {
		if (skeleton.unique_bones.find(node->mName.C_Str()) != skeleton.unique_bones.end()) {
			traverse_bone(node, skeleton, Skeleton::NullIndex);
		}
		else {
			for (u32 node_index = 0; node_index < node->mNumChildren; node_index++) {
				traverse_node(node->mChildren[node_index], skeleton);
			}
		}
	}

	bool extract_skeleton(const aiScene* scene, Skeleton& skeleton) {
		// get all the bone names into the set
		for (u32 meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];

			for (u32 boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
				skeleton.unique_bones.emplace(mesh->mBones[boneIndex]->mName.C_Str());
			}
		}

		skeleton.num_bones = skeleton.unique_bones.size();
		if (skeleton.num_bones == 0) {
			return false;
		}
		skeleton.bones.reserve(skeleton.num_bones);

		traverse_node(scene->mRootNode, skeleton);
		skeleton.num_bones = skeleton.bones.size();

		// all nodes are loaded, loop through and calculate the model matrix
		skeleton.bones[0].inv_model_matrix = skeleton.bones[0].local_matrix;
		for (u32 n = 1; n < skeleton.num_bones; n++) {
			u32 parent_idx = skeleton.bones[n].parent_idx;
			skeleton.bones[n].inv_model_matrix = laml::mul(skeleton.bones[parent_idx].inv_model_matrix, skeleton.bones[n].local_matrix);
		}
		// calc inverse model matrices
		for (u32 n = 0; n < skeleton.num_bones; n++) {
			skeleton.bones[n].inv_model_matrix = laml::inverse(skeleton.bones[n].inv_model_matrix);
		}

		return true;
	}

	u32 get_bone_index(const Skeleton& skeleton, const std::string_view name) {
		for (u32 i = 0; i < skeleton.bones.size(); i++) {
			const auto& bone = skeleton.bones[i];
			if (bone.name == name) {
				return static_cast<u32>(i);
			}
		}

		return Skeleton::NullIndex;
	}
}