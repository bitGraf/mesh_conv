#include "animimporter.h"

#include <set>
#include <map>
#include <assimp/postprocess.h>

#include "utils.h"

namespace rh {

	class BoneHierarchy
	{
	public:
		BoneHierarchy(const aiScene* scene);

		void ExtractBones();
		void TraverseNode(aiNode* node, Skeleton* skeleton);
		void TraverseBone(aiNode* node, Skeleton* skeleton, uint32_t parentIndex);
		Skeleton* CreateSkeleton();

	private:
		std::set<std::string_view> m_Bones;
		const aiScene* m_Scene;
	};


	Skeleton* ImportSkeleton(const std::string_view filename)
	{
		//AssimpLogStream::Initialize();

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filename.data(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_PopulateArmatureData | aiProcess_GlobalScale);
		return ImportSkeleton(scene);
	}


	Skeleton* ImportSkeleton(const aiScene* scene)
	{
		BoneHierarchy boneHierarchy(scene);
		return boneHierarchy.CreateSkeleton();
	}


	Animation* ImportAnimation(const std::string_view filename, const Skeleton* skeleton)
	{
		//AssimpLogStream::Initialize();

		printf("Animation: Loading animation: %s\n", filename.data());
		Animation* animation = nullptr;

		if (skeleton->GetNumBones() == 0)
		{
			printf("Animation: Empty skeleton passed to animation asset for file '%s'\n", filename.data());
			return animation;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filename.data(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_PopulateArmatureData | aiProcess_GlobalScale);
		if (!scene || !scene->HasAnimations())
		{
			printf("Animation: Failed to load animation from file '%s'\n", filename.data());
			return animation;
		}

		// If the file contains a skeleton, check is same as skeleton we want to apply the animation to
		// TODO: Later we might want some sort of re-targeting functionality to deal with this.
		auto localSkeleton = ImportSkeleton(scene);
		if (localSkeleton && localSkeleton != skeleton)
		{
			printf("Animation: Skeleton found in animation file '%s' differs from expected.  All animations in an animation controller must share the same skeleton!\n", filename.data());
			return animation;
		}

		auto animationNames = GetAnimationNames(scene);
		if (animationNames.empty())
		{
			// shouldnt ever get here, since we already checked scene.HasAnimations()...
			printf("Animation: Failed to load animation from file: %s\n", filename.data());
			return animation;
		}

		if (animationNames.size() > 1)
		{
			printf("Animation: File '%s' contains %i animations.  Only the first will be imported\n", filename.data(), (u32)animationNames.size());
		}

		aiString animationName;
		animation = ImportAnimation(scene, animationNames.front(), skeleton);
		if (!animation)
		{
			printf("Animation, Failed to extract animation '%s' from file '%s'\n", animationNames.front().c_str(), filename.data());
		}
		return animation;
	}


	std::vector<std::string> GetAnimationNames(const aiScene* scene)
	{
		std::vector<std::string> animationNames;
		if (scene)
		{
			animationNames.reserve(scene->mNumAnimations);
			for (size_t i = 0; i < scene->mNumAnimations; ++i)
			{
				animationNames.emplace_back(scene->mAnimations[i]->mName.C_Str());
			}
		}
		return animationNames;
	}


	template<typename T> struct KeyFrame
	{
		f32 FrameTime;
		T Value;
		KeyFrame(const f32 frameTime, const T& value) : FrameTime(frameTime), Value(value) {}
	};


	struct Channel
	{
		std::vector<KeyFrame<laml::Vec3>> Translations;
		std::vector<KeyFrame<laml::Quat>> Rotations;
		std::vector<KeyFrame<laml::Vec3>> Scales;
		u32 Index;
	};


	// Import all of the channels from anim that refer to bones in skeleton
	static auto ImportChannels(const aiAnimation* anim, const Skeleton* skeleton)
	{
		std::vector<Channel> channels;

		std::unordered_map<std::string_view, uint32_t> boneIndices;
		for (uint32_t i = 0; i < skeleton->GetNumBones(); ++i)
		{
			boneIndices.emplace(skeleton->GetBoneName(i), i);
		}

		std::map<u32, aiNodeAnim*> validChannels;
		for (uint32_t channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex)
		{
			aiNodeAnim* nodeAnim = anim->mChannels[channelIndex];
			auto it = boneIndices.find(nodeAnim->mNodeName.C_Str());
			if (it != boneIndices.end())
			{
				validChannels.emplace(it->second, nodeAnim);
			}
		}

		channels.resize(skeleton->GetNumBones());
		for (uint32_t boneIndex = 0; boneIndex < skeleton->GetNumBones(); ++boneIndex)
		{
			channels[boneIndex].Index = boneIndex;
			if (auto validChannel = validChannels.find(boneIndex); validChannel != validChannels.end())
			{
				auto nodeAnim = validChannel->second;
				channels[boneIndex].Translations.reserve(nodeAnim->mNumPositionKeys + 2); // +2 because worst case we insert two more keys
				channels[boneIndex].Rotations.reserve(nodeAnim->mNumRotationKeys + 2);
				channels[boneIndex].Scales.reserve(nodeAnim->mNumScalingKeys + 2);

				// Note: There is no need to check for duplicate keys (i.e. multiple keys all at same frame time)
				//       because Assimp throws these out for us
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumPositionKeys; ++keyIndex)
				{
					aiVectorKey key = nodeAnim->mPositionKeys[keyIndex];
					float frameTime = std::clamp(static_cast<float>(key.mTime / anim->mDuration), 0.0f, 1.0f);
					if ((keyIndex == 0) && (frameTime > 0.0f))
					{
						channels[boneIndex].Translations.emplace_back(0.0f, laml::Vec3{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z) });
					}
					channels[boneIndex].Translations.emplace_back(frameTime, laml::Vec3{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z) });
				}
				if (channels[boneIndex].Translations.empty())
				{
					printf("Animation: No translation track found for bone '%s'\n", skeleton->GetBoneName(boneIndex).c_str());
					channels[boneIndex].Translations = { {0.0f, laml::Vec3{0.0f}}, {1.0f, laml::Vec3{0.0f}} };
				}
				else if (channels[boneIndex].Translations.back().FrameTime < 1.0f)
				{
					channels[boneIndex].Translations.emplace_back(1.0, channels[boneIndex].Translations.back().Value);
				}
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumRotationKeys; ++keyIndex)
				{
					aiQuatKey key = nodeAnim->mRotationKeys[keyIndex];
					float frameTime = std::clamp(static_cast<float>(key.mTime / anim->mDuration), 0.0f, 1.0f);

					// WARNING: constructor parameter order for a quat is still WXYZ even if you have defined GLM_FORCE_QUAT_DATA_XYZW
					if ((keyIndex == 0) && (frameTime > 0.0f))
					{
						channels[boneIndex].Rotations.emplace_back(0.0f, laml::Quat{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z), static_cast<float>(key.mValue.w) });
					}
					channels[boneIndex].Rotations.emplace_back(frameTime, laml::Quat{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z), static_cast<float>(key.mValue.w) });
					assert(fabs(laml::length(channels[boneIndex].Rotations.back().Value) - 1.0f) < 0.00001f);   // check rotations are normalized (I think assimp ensures this, but not 100% sure)
				}
				if (channels[boneIndex].Rotations.empty())
				{
					printf("Animation: No rotation track found for bone '%s'\n", skeleton->GetBoneName(boneIndex).c_str());
					channels[boneIndex].Rotations = { {0.0f, laml::Quat{0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, laml::Quat{0.0f, 0.0f, 0.0f, 1.0f}} };
				}
				else if (channels[boneIndex].Rotations.back().FrameTime < 1.0f)
				{
					channels[boneIndex].Rotations.emplace_back(1.0, channels[boneIndex].Rotations.back().Value);
				}
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumScalingKeys; ++keyIndex)
				{
					aiVectorKey key = nodeAnim->mScalingKeys[keyIndex];
					float frameTime = std::clamp(static_cast<float>(key.mTime / anim->mDuration), 0.0f, 1.0f);
					if (keyIndex == 0 && frameTime > 0.0f)
					{
						channels[boneIndex].Scales.emplace_back(0.0f, laml::Vec3{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z) });
					}
					channels[boneIndex].Scales.emplace_back(frameTime, laml::Vec3{ static_cast<float>(key.mValue.x), static_cast<float>(key.mValue.y), static_cast<float>(key.mValue.z) });
				}
				if (channels[boneIndex].Scales.empty())
				{
					printf("Animation: No scale track found for bone '%s'\n", skeleton->GetBoneName(boneIndex).c_str());
					channels[boneIndex].Scales = { {0.0f, laml::Vec3{1.0f}}, {1.0f, laml::Vec3{1.0f}} };
				}
				else if (channels[boneIndex].Scales.back().FrameTime < 1.0f)
				{
					channels[boneIndex].Scales.emplace_back(1.0, channels[boneIndex].Scales.back().Value);
				}
			}
			else
			{
				printf("Animation: No animation tracks found for bone '%s'\n", skeleton->GetBoneName(boneIndex).c_str());
				channels[boneIndex].Translations = { {0.0f, laml::Vec3{0.0f}}, {1.0f, laml::Vec3{0.0f}} };
				channels[boneIndex].Rotations = { {0.0f, laml::Quat{0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, laml::Quat{0.0f, 0.0f, 0.0f, 1.0f}} };
				channels[boneIndex].Scales = { {0.0f, laml::Vec3{1.0f}}, {1.0f, laml::Vec3{1.0f}} };
			}
		}

		return channels;
	}


	static auto ConcatenateChannelsAndSort(const std::vector<Channel>& channels)
	{
		// We concatenate the translations for all the channels into one big long vector, and then sort
		// it on _previous_ frame time.  This gives us an efficient way to sample the key frames later on.
		// (taking advantage of fact that animation almost always plays forwards)

		uint32_t numTranslations = 0;
		uint32_t numRotations = 0;
		uint32_t numScales = 0;

		for (auto channel : channels)
		{
			numTranslations += static_cast<uint32_t>(channel.Translations.size());
			numRotations += static_cast<uint32_t>(channel.Rotations.size());
			numScales += static_cast<uint32_t>(channel.Scales.size());
		}

		std::vector<std::pair<float, TranslationKey>> translationKeysTemp;
		std::vector<std::pair<float, RotationKey>> rotationKeysTemp;
		std::vector<std::pair<float, ScaleKey>> scaleKeysTemp;
		translationKeysTemp.reserve(numTranslations);
		rotationKeysTemp.reserve(numRotations);
		scaleKeysTemp.reserve(numScales);
		for (const auto& channel : channels)
		{
			float prevFrameTime = -1.0f;
			for (const auto& translation : channel.Translations)
			{
				translationKeysTemp.emplace_back(prevFrameTime, TranslationKey{ translation.FrameTime, channel.Index, translation.Value });
				prevFrameTime = translation.FrameTime;
			}

			prevFrameTime = -1.0f;
			for (const auto& rotation : channel.Rotations)
			{
				rotationKeysTemp.emplace_back(prevFrameTime, RotationKey{ rotation.FrameTime, channel.Index, rotation.Value });
				prevFrameTime = rotation.FrameTime;
			}

			prevFrameTime = -1.0f;
			for (const auto& scale : channel.Scales)
			{
				scaleKeysTemp.emplace_back(prevFrameTime, ScaleKey{ scale.FrameTime, channel.Index, scale.Value });
				prevFrameTime = scale.FrameTime;
			}
		}
		std::sort(translationKeysTemp.begin(), translationKeysTemp.end(), [](const auto& a, const auto& b) { return (a.first < b.first) || ((a.first == b.first) && a.second.Track < b.second.Track); });
		std::sort(rotationKeysTemp.begin(), rotationKeysTemp.end(), [](const auto& a, const auto& b) { return (a.first < b.first) || ((a.first == b.first) && a.second.Track < b.second.Track); });
		std::sort(scaleKeysTemp.begin(), scaleKeysTemp.end(), [](const auto& a, const auto& b) { return (a.first < b.first) || ((a.first == b.first) && a.second.Track < b.second.Track); });

		return std::tuple{ translationKeysTemp, rotationKeysTemp, scaleKeysTemp };
	}


	static auto ExtractKeys(const std::vector<std::pair<float, TranslationKey>>& translationKeysTemp, const std::vector<std::pair<float, RotationKey>>& rotationKeysTemp, const std::vector<std::pair<float, ScaleKey>>& scaleKeysTemp)
	{
		std::vector<TranslationKey> translationKeys;
		std::vector<RotationKey> rotationKeys;
		std::vector<ScaleKey> scaleKeys;
		translationKeys.reserve(translationKeysTemp.size());
		rotationKeys.reserve(rotationKeysTemp.size());
		scaleKeys.reserve(scaleKeysTemp.size());
		for (const auto& translation : translationKeysTemp)
		{
			translationKeys.emplace_back(translation.second);
		}
		for (const auto& rotation : rotationKeysTemp)
		{
			rotationKeys.emplace_back(rotation.second);
		}
		for (const auto& scale : scaleKeysTemp)
		{
			scaleKeys.emplace_back(scale.second);
		}

		return std::tuple{ translationKeys, rotationKeys, scaleKeys };
	}


	Animation* ImportAnimation(const aiScene* scene, const std::string_view animationName, const Skeleton* skeleton)
	{
		if (!scene)
		{
			return nullptr;
		}

		Animation* animation = nullptr;

		for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
		{
			const aiAnimation* anim = scene->mAnimations[animIndex];
			if (animationName == anim->mName.C_Str())
			{
				auto channels = ImportChannels(anim, skeleton);
				auto [translationKeysTemp, rotationKeysTemp, scaleKeysTemp] = ConcatenateChannelsAndSort(channels);
				auto [translationKeys, rotationKeys, scaleKeys] = ExtractKeys(translationKeysTemp, rotationKeysTemp, scaleKeysTemp);

				double samplingRate = anim->mTicksPerSecond;
				if (samplingRate < 0.0001)
				{
					samplingRate = 1.0;
				}

				animation = new Animation(animationName, static_cast<float>(anim->mDuration / samplingRate));
				animation->SetKeyFrames(std::move(translationKeys), std::move(rotationKeys), std::move(scaleKeys));
				break;
			}
		}
		return animation;
	}


	BoneHierarchy::BoneHierarchy(const aiScene* scene) : m_Scene(scene)
	{
	}


	Skeleton* BoneHierarchy::CreateSkeleton()
	{
		if (!m_Scene)
		{
			return nullptr;
		}

		ExtractBones();
		if (m_Bones.empty())
		{
			return nullptr;
		}

		auto skeleton = new Skeleton(static_cast<uint32_t>(m_Bones.size()));
		TraverseNode(m_Scene->mRootNode, skeleton);

		return skeleton;
	}


	void BoneHierarchy::ExtractBones()
	{
		// Note: ASSIMP does not appear to support import of digital content files that contain _only_ an armature/skeleton and no mesh.
		for (uint32_t meshIndex = 0; meshIndex < m_Scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = m_Scene->mMeshes[meshIndex];
			for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				m_Bones.emplace(mesh->mBones[boneIndex]->mName.C_Str());
			}
		}
	}


	void BoneHierarchy::TraverseNode(aiNode* node, Skeleton* skeleton)
	{
		if (m_Bones.find(node->mName.C_Str()) != m_Bones.end())
		{
			TraverseBone(node, skeleton, Skeleton::NullIndex);
		}
		else
		{
			for (uint32_t nodeIndex = 0; nodeIndex < node->mNumChildren; ++nodeIndex)
			{
				TraverseNode(node->mChildren[nodeIndex], skeleton);
			}
		}
	}


	void BoneHierarchy::TraverseBone(aiNode* node, Skeleton* skeleton, uint32_t parentIndex)
	{
		uint32_t boneIndex = skeleton->AddBone(node->mName.C_Str(), parentIndex, utils::mat4_from_aiMatrix4x4(node->mTransformation));
		for (uint32_t nodeIndex = 0; nodeIndex < node->mNumChildren; ++nodeIndex)
		{
			TraverseBone(node->mChildren[nodeIndex], skeleton, boneIndex);
		}
	}
}