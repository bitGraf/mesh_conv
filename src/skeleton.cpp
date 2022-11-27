#include "skeleton.h"
#include "utils.h"

#include <set>

namespace rh {

	Skeleton::Skeleton(u32 size) {
		m_BoneNames.reserve(size);
		m_ParentBoneIndices.reserve(size);
	}

	u32 Skeleton::AddBone(std::string name, u32 parentIndex, const laml::Mat4& transform)
	{
		u32 index = static_cast<u32>(m_BoneNames.size());
		m_BoneNames.emplace_back(name);
		m_ParentBoneIndices.emplace_back(parentIndex);
		m_BoneTranslations.emplace_back();
		m_BoneRotations.emplace_back();
		m_BoneScales.emplace_back();
		laml::Mat3 rot_mat;
		laml::transform::decompose(transform, rot_mat, m_BoneTranslations.back(), m_BoneScales.back());
		m_BoneRotations.back() = laml::transform::quat_from_mat(rot_mat);

		return index;
	}

	u32 Skeleton::GetBoneIndex(const std::string_view name) const
	{
		for (size_t i = 0; i < m_BoneNames.size(); ++i)
		{
			if (m_BoneNames[i] == name)
			{
				return static_cast<u32>(i);
			}
		}
		return Skeleton::NullIndex;
	}

	bool Skeleton::operator==(const Skeleton& other) const
	{
		bool areSame = false;
		if (GetNumBones() == other.GetNumBones())
		{
			areSame = true;
			for (u32 i = 0; i < GetNumBones(); ++i)
			{
				if (GetBoneName(i) != other.GetBoneName(i))
				{
					areSame = false;
					break;
				}
			}
		}
		return areSame;
	}
}