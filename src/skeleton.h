#pragma once

#include <assimp/scene.h>
#include <cassert>
#include <vector>
#include <laml/laml.hpp>

namespace rh {

	class Skeleton {
	public:
		static const u32 NullIndex = ~0;

	public:
		Skeleton(u32 size);

		u32 AddBone(std::string name, u32 parentIndex, const laml::Mat4& transform);
		u32 GetBoneIndex(const std::string_view name) const;

		u32 GetParentBoneIndex(const u32 boneIndex) const {
			if (!(boneIndex < m_ParentBoneIndices.size())) {
				printf("bone index out of range in Skeleton::GetParentIndex()!");
				assert(false);
			}
			return m_ParentBoneIndices[boneIndex];
		}

		u32 GetNumBones() const { return static_cast<u32>(m_BoneNames.size()); }
		const std::string& GetBoneName(const u32 boneIndex) const {
			if (!(boneIndex < m_BoneNames.size())) {
				printf("bone index out of range in Skeleton::GetBoneName()!");
			}
			return m_BoneNames[boneIndex];
		}
		const auto& GetBoneNames() const { return m_BoneNames; }

		const std::vector<laml::Vec3> GetBoneTranslations() const { return m_BoneTranslations; }
		const std::vector<laml::Quat> GetBoneRotations() const { return m_BoneRotations; }
		const std::vector<laml::Vec3> GetBoneScales() const { return m_BoneScales; }

		bool operator==(const Skeleton& other) const;

		bool operator!=(const Skeleton& other) const { return !(*this == other); }

	private:
		std::vector<std::string> m_BoneNames;
		std::vector<u32> m_ParentBoneIndices;

		// rest pose of skeleton. All in bone-local space (i.e. translation/rotation/scale relative to parent)
		std::vector<laml::Vec3> m_BoneTranslations;
		std::vector<laml::Quat> m_BoneRotations;
		std::vector<laml::Vec3> m_BoneScales;
	};
}