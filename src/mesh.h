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
		laml::Vec3 Position;
		laml::Vec3 Normal;
		laml::Vec3 Tangent;
		laml::Vec3 Bitangent;
		laml::Vec2 Texcoord;
	};

	struct BoneInfo
	{
		laml::Mat4 SubMeshInverseTransform;
		laml::Mat4 InverseBindPose;
		uint32_t SubMeshIndex;
		uint32_t BoneIndex;

		BoneInfo(laml::Mat4 subMeshInverseTransform, laml::Mat4 inverseBindPose, uint32_t subMeshIndex, uint32_t boneIndex)
			: SubMeshInverseTransform(subMeshInverseTransform)
			, InverseBindPose(inverseBindPose)
			, SubMeshIndex(subMeshIndex)
			, BoneIndex(boneIndex)
		{}
	};

	struct BoneInfluence
	{
		u32 BoneInfoIndices[4] = { 0, 0, 0, 0 };
		f32 Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		void AddBoneData(u32 boneInfoIndex, f32 weight)
		{
			if (weight < 0.0f || weight > 1.0f)
			{
				printf("Vertex bone weight is out of range. We will clamp it to [0, 1] (BoneID={%i}, Weight={%f})", boneInfoIndex, weight);
				weight = std::clamp(weight, 0.0f, 1.0f);
			}
			if (weight > 0.0f)
			{
				for (size_t i = 0; i < 4; i++)
				{
					if (Weights[i] == 0.0f)
					{
						BoneInfoIndices[i] = boneInfoIndex;
						Weights[i] = weight;
						return;
					}
				}

				// Note: when importing from assimp we are passing aiProcess_LimitBoneWeights which automatically keeps only the top N (where N defaults to 4)
				//       bone weights (and normalizes the sum to 1), which is exactly what we want.
				//       So, we should never get here.
				printf("Vertex has more than four bones affecting it, extra bone influences will be discarded (BoneID={%i}, Weight={%f})", boneInfoIndex, weight);
			}
		}

		void NormalizeWeights()
		{
			f32 sumWeights = 0.0f;
			for (size_t i = 0; i < 4; i++)
			{
				sumWeights += Weights[i];
			}
			if (sumWeights > 0.0f)
			{
				for (size_t i = 0; i < 4; i++)
				{
					Weights[i] /= sumWeights;
				}
			}
		}
	};

	struct Index
	{
		u32 V1, V2, V3;
	};

	struct Submesh {
		u32 BaseVertex;
		u32 BaseIndex;
		u32 MaterialIndex;
		u32 IndexCount;
		u32 VertexCount;

		laml::Mat4 Transform; // World Transform
		laml::Mat4 LocalTransform;

		std::string NodeName, MeshName;
		bool IsRigged = false;
	};

	class MeshSource {
	public:
		MeshSource(const std::string& filename);
		~MeshSource();

		//void DumpVertexBuffer();

		std::vector<Submesh>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<Index>& GetIndices() const { return m_Indices; }

		bool HasSkeleton() const { return m_Skeleton!= nullptr; }
		bool IsSubmeshRigged(uint32_t submeshIndex) const { return m_Submeshes[submeshIndex].IsRigged; }
		const Skeleton* GetSkeleton() const { assert(m_Skeleton && "Attempted to access null skeleton!"); return m_Skeleton;  }
		bool IsCompatibleSkeleton(const u32 animationIndex, const Skeleton* skeleton) const;
		uint32_t GetAnimationCount() const;
		const Animation* GetAnimation(const u32 animationIndex, const Skeleton* skeleton) const;
		const std::vector<BoneInfluence>& GetBoneInfluences() const { return m_BoneInfluences; }

		//std::vector<Material*>& GetMaterials() { return m_Materials; }
		//const std::vector<Ref<Material>>& GetMaterials() const { return m_Materials; }
		const std::string& GetFilePath() const { return m_FilePath; }

	private:
		void TraverseNodes(aiNode* node, const laml::Mat4& parentTransform = laml::Mat4(1.0f), u32 level = 0);

	private:
		std::vector<Submesh> m_Submeshes;

		Assimp::Importer* m_Importer; // note: the importer owns data pointed to by m_Scene, and m_NodeMap and hence must stay in scope for lifetime of MeshSource.

		std::vector<Vertex> m_Vertices;
		std::vector<Index> m_Indices;
		std::unordered_map<aiNode*, std::vector<uint32_t>> m_NodeMap;
		const aiScene* m_Scene;

		std::vector<BoneInfluence> m_BoneInfluences;
		std::vector<BoneInfo> m_BoneInfo;
		mutable Skeleton* m_Skeleton;
		mutable std::vector<Animation*> m_Animations;

		//std::vector<Ref<Material>> m_Materials;

		std::string m_FilePath;
	};
}