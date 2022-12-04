#include "skeleton.h"
#include "utils.h"

#include <set>

namespace rh {

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