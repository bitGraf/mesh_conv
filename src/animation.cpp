#include "animation.h"

#include <unordered_map>
#include <map>
#include "utils.h"

namespace rh {

	u32 find_bone_index(const std::string& node_name, const Skeleton& skeleton) {
		for (u32 bone_idx = 0; bone_idx < skeleton.num_bones; bone_idx++) {
			const auto& bone = skeleton.bones[bone_idx];

			if (bone.name == node_name) {
				return bone_idx;
			}
		}

		assert(false && "Could not find node_name in the skeleton!");
		return ~0;
	}

	bool extract_animation(const aiAnimation* s_anim, Animation& anim, const Skeleton& skeleton) {

		//std::unordered_map<std::string_view, u32> bone_indices;
		//for (u32 n = 0; n < skeleton.num_bones; n++) {
		//	bone_indices.emplace(skeleton.bones[n].name, n);
		//}

		//u32 num_channels = s_anim->mNumChannels;
		//std::map<u32, const aiNodeAnim*> validChannels;
		//for (u32 chan_idx = 0; chan_idx < num_channels; chan_idx++) {
		//	const aiNodeAnim* s_nodeAnim = s_anim->mChannels[chan_idx];
		//
		//	auto it = bone_indices.find(s_nodeAnim->mNodeName.C_Str());
		//	if (it != bone_indices.end()) {
		//		validChannels.emplace(it->second, s_nodeAnim);
		//	}
		//}

		//here
		anim.name = s_anim->mName.C_Str();
		anim.duration   = s_anim->mDuration;		// # of "frames" in the animation
		anim.frame_rate =  s_anim->mTicksPerSecond;	// # of "frames" per second
		anim.num_nodes = skeleton.num_bones;
		anim.nodes.resize(anim.num_nodes);

		std::unordered_map<std::string_view, u32> channel_indices;
		for (u32 channel_idx = 0; channel_idx < s_anim->mNumChannels; channel_idx++) {
			const auto* chan = s_anim->mChannels[channel_idx];
			channel_indices.emplace(chan->mNodeName.C_Str(), channel_idx);
		}

		for (u32 node_idx = 0; node_idx < anim.num_nodes; node_idx++) {
			// find channel_idx that matches bone_idx
			// resolve by node names
			const auto bone_name = skeleton.bones[node_idx].name;
			
			auto it = channel_indices.find(bone_name);
			if (it == channel_indices.end()) {
				// no animation channel for this bone -> fill with default values

				anim.nodes[node_idx].flag = 0;
				anim.nodes[node_idx].translations.num_samples = 2;
				anim.nodes[node_idx].translations.values.push_back(laml::Vec3(0.0f, 0.0f, 0.0f));
				anim.nodes[node_idx].translations.frame_time.push_back(0.0f);
				anim.nodes[node_idx].translations.values.push_back(laml::Vec3(0.0f, 0.0f, 0.0f));
				anim.nodes[node_idx].translations.frame_time.push_back(anim.duration);

				anim.nodes[node_idx].flag = 0;
				anim.nodes[node_idx].rotations.num_samples = 2;
				anim.nodes[node_idx].rotations.values.push_back(laml::Quat(0.0f, 0.0f, 0.0f, 1.0f));
				anim.nodes[node_idx].rotations.frame_time.push_back(0.0f);
				anim.nodes[node_idx].rotations.values.push_back(laml::Quat(0.0f, 0.0f, 0.0f, 1.0f));
				anim.nodes[node_idx].rotations.frame_time.push_back(anim.duration);

				anim.nodes[node_idx].flag = 0;
				anim.nodes[node_idx].scales.num_samples = 2;
				anim.nodes[node_idx].scales.values.push_back(laml::Vec3(1.0f, 1.0f, 1.0f));
				anim.nodes[node_idx].scales.frame_time.push_back(0.0f);
				anim.nodes[node_idx].scales.values.push_back(laml::Vec3(1.0f, 1.0f, 1.0f));
				anim.nodes[node_idx].scales.frame_time.push_back(anim.duration);
			} else {
				auto channel_idx = it->second;
				const aiNodeAnim* node = s_anim->mChannels[channel_idx];

				anim.nodes[node_idx].flag = 0;

				anim.nodes[node_idx].translations.num_samples = node->mNumPositionKeys;
				for (u32 f = 0; f < node->mNumPositionKeys; f++) {
					const aiVectorKey& frame = node->mPositionKeys[f];

					anim.nodes[node_idx].translations.values.push_back(utils::vec3_from_aiVector3D(frame.mValue));
					anim.nodes[node_idx].translations.frame_time.push_back(frame.mTime);
				}

				anim.nodes[node_idx].scales.num_samples = node->mNumScalingKeys;
				for (u32 f = 0; f < node->mNumScalingKeys; f++) {
					const aiVectorKey& frame = node->mScalingKeys[f];

					anim.nodes[node_idx].scales.values.push_back(utils::vec3_from_aiVector3D(frame.mValue));
					anim.nodes[node_idx].scales.frame_time.push_back(frame.mTime);
				}

				anim.nodes[node_idx].rotations.num_samples = node->mNumRotationKeys;
				for (u32 f = 0; f < node->mNumRotationKeys; f++) {
					const aiQuatKey& frame = node->mRotationKeys[f];

					anim.nodes[node_idx].rotations.values.push_back(utils::quat_from_aiQuaternion(frame.mValue));
					anim.nodes[node_idx].rotations.frame_time.push_back(frame.mTime);
				}
			}
		}

		return true;
	}
}