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
}