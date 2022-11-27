#include "mesh.h"

#include <set>

#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/postprocess.h>

#include "animation.h"
#include "utils.h"
#include "animimporter.h"

namespace rh {

	MeshSource::~MeshSource() {

	}

    MeshSource::MeshSource(const std::string& filename)
		: m_FilePath(filename)
	{
		//AssimpLogStream::Initialize();

		printf("Mesh: Loading mesh: %s\n", filename.c_str());

		m_Importer = new Assimp::Importer();
		m_Importer->SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		m_Importer->SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		const aiScene* scene = m_Importer->ReadFile(filename, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_PopulateArmatureData | aiProcess_GlobalScale);
		if (!scene /* || !scene->HasMeshes()*/)  // note: scene can legit contain no meshes (e.g. it could contain an armature, an animation, and no skin (mesh)))
		{
			printf("Mesh: Failed to load mesh file: %s\n", filename.c_str());
			return;
		}

		m_Scene = scene;

		m_Skeleton = ImportSkeleton(scene);
		printf("Animation: Skeleton%sfound in mesh file '%s'\n", HasSkeleton()? " " : " not ", filename.c_str());
		if (HasSkeleton())
		{
			const auto animationNames = GetAnimationNames(scene);
			m_Animations.reserve(std::size(animationNames));
			for (const auto& animationName : animationNames)
			{
				m_Animations.emplace_back(ImportAnimation(scene, animationName, m_Skeleton));
			}
		}

		// If no meshes in the scene, there's nothing more for us to do
		if (!scene->HasMeshes())
		{
			return;
		}

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		m_Submeshes.reserve(scene->mNumMeshes);
		for (unsigned m = 0; m < scene->mNumMeshes; m++)
		{
			aiMesh* mesh = scene->mMeshes[m];

			Submesh& submesh = m_Submeshes.emplace_back();
			submesh.BaseVertex = vertexCount;
			submesh.BaseIndex = indexCount;
			submesh.MaterialIndex = mesh->mMaterialIndex;
			submesh.VertexCount = mesh->mNumVertices;
			submesh.IndexCount = mesh->mNumFaces * 3;
			submesh.MeshName = mesh->mName.C_Str();
			submesh.Transform = laml::Mat4(1.0f);
			submesh.LocalTransform = laml::Mat4(1.0f);

			vertexCount += mesh->mNumVertices;
			indexCount += submesh.IndexCount;

			assert(mesh->HasPositions() && "Meshes require positions.");
			assert(mesh->HasNormals() && "Meshes require normals.");

			// Vertices
			for (size_t i = 0; i < mesh->mNumVertices; i++)
			{
				Vertex vertex;
				vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
				vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

				if (mesh->HasTangentsAndBitangents())
				{
					vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
					vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
				}

				if (mesh->HasTextureCoords(0))
					vertex.Texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

				m_Vertices.push_back(vertex);
			}

			// Indices
			for (size_t i = 0; i < mesh->mNumFaces; i++)
			{
				assert(mesh->mFaces[i].mNumIndices == 3 && "Must have 3 indices.");
				Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
				m_Indices.push_back(index);
			}
		}

#if MESH_DEBUG_LOG
		HZ_CORE_INFO_TAG("Mesh", "Traversing nodes for scene '{0}'", filename);
		Utils::PrintNode(scene->mRootNode, 0);
#endif

		TraverseNodes(scene->mRootNode);

		// Bones
		if (HasSkeleton())
		{
			m_BoneInfluences.resize(m_Vertices.size());
			for (uint32_t m = 0; m < scene->mNumMeshes; m++)
			{
				aiMesh* mesh = scene->mMeshes[m];
				Submesh& submesh = m_Submeshes[m];

				if (mesh->mNumBones > 0)
				{
					submesh.IsRigged = true;
					for (uint32_t i = 0; i < mesh->mNumBones; i++)
					{
						aiBone* bone = mesh->mBones[i];
						bool hasNonZeroWeight = false;
						for (size_t j = 0; j < bone->mNumWeights; j++)
						{
							if (bone->mWeights[j].mWeight > 0.000001f)
							{
								hasNonZeroWeight = true;
							}
						}
						if (!hasNonZeroWeight)
							continue;

						// Find bone in skeleton
						uint32_t boneIndex = m_Skeleton->GetBoneIndex(bone->mName.C_Str());
						if (boneIndex == Skeleton::NullIndex)
						{
							printf("Animation: Could not find mesh bone '%s' in skeleton!\n", bone->mName.C_Str());
						}

						uint32_t boneInfoIndex = ~0;
						for (size_t j = 0; j < m_BoneInfo.size(); ++j)
						{
							// note: Same bone could influence different submeshes (and each will have different transforms in the bind pose).
							//       Hence the need to differentiate on submesh index here.
							if ((m_BoneInfo[j].BoneIndex == boneIndex) && (m_BoneInfo[j].SubMeshIndex == m))
							{
								boneInfoIndex = static_cast<uint32_t>(j);
								break;
							}
						}
						if (boneInfoIndex == ~0)
						{
							boneInfoIndex = static_cast<uint32_t>(m_BoneInfo.size());
							m_BoneInfo.emplace_back(laml::inverse(submesh.Transform), utils::mat4_from_aiMatrix4x4(bone->mOffsetMatrix), m, boneIndex);
						}

						for (size_t j = 0; j < bone->mNumWeights; j++)
						{
							int VertexID = submesh.BaseVertex + bone->mWeights[j].mVertexId;
							float Weight = bone->mWeights[j].mWeight;
							m_BoneInfluences[VertexID].AddBoneData(boneInfoIndex, Weight);
						}
					}
				}
			}

			for (auto& boneInfluence: m_BoneInfluences) {
				boneInfluence.NormalizeWeights();
			}
		}
	}

	//void MeshSource::DumpVertexBuffer()
	//{
	//	// TODO: Convert to ImGui
	//	HZ_MESH_LOG("------------------------------------------------------");
	//	HZ_MESH_LOG("Vertex Buffer Dump");
	//	HZ_MESH_LOG("Mesh: {0}", m_FilePath);
	//	for (size_t i = 0; i < m_Vertices.size(); i++)
	//	{
	//		auto& vertex = m_Vertices[i];
	//		HZ_MESH_LOG("Vertex: {0}", i);
	//		HZ_MESH_LOG("Position: {0}, {1}, {2}", vertex.Position.x, vertex.Position.y, vertex.Position.z);
	//		HZ_MESH_LOG("Normal: {0}, {1}, {2}", vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
	//		HZ_MESH_LOG("Binormal: {0}, {1}, {2}", vertex.Binormal.x, vertex.Binormal.y, vertex.Binormal.z);
	//		HZ_MESH_LOG("Tangent: {0}, {1}, {2}", vertex.Tangent.x, vertex.Tangent.y, vertex.Tangent.z);
	//		HZ_MESH_LOG("TexCoord: {0}, {1}", vertex.Texcoord.x, vertex.Texcoord.y);
	//		HZ_MESH_LOG("--");
	//	}
	//	HZ_MESH_LOG("------------------------------------------------------");
	//}

	// TODO (0x): this is temporary.. and will eventually be replaced with some kind of skeleton retargeting
	bool MeshSource::IsCompatibleSkeleton(const u32 animationIndex, const Skeleton* skeleton) const
	{
		if (m_Skeleton)
		{
			return m_Skeleton->GetBoneNames() == skeleton->GetBoneNames();
		}
		else
		{
			// No skeleton means this meshsource contains just animations (and no skin).  In this case
			// assimp does not import the bones and we will (not yet) have a skeleton.
			// In this case, we have a quick look at the channels in the animation and decide
			// whether or not they look relevant for the given skeleton.

			if (!m_Scene || (m_Scene->mNumAnimations <= animationIndex))
			{
				return false;
			}

			//const auto animationNames = AnimationImporterAssimp::GetAnimationNames(m_Scene);
			//if (animationNames.empty())
			//{
			//	return false;
			//}

			const aiAnimation* anim = m_Scene->mAnimations[animationIndex];

			std::unordered_map<std::string_view, uint32_t> boneIndices;
			for (uint32_t i = 0; i < skeleton->GetNumBones(); ++i)
			{
				boneIndices.emplace(skeleton->GetBoneName(i), i);
			}

			std::set<std::tuple<u32, aiNodeAnim*>> validChannels;
			for (uint32_t channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex)
			{
				aiNodeAnim* nodeAnim = anim->mChannels[channelIndex];
				auto it = boneIndices.find(nodeAnim->mNodeName.C_Str());
				if (it != boneIndices.end())
				{
					validChannels.emplace(it->second, nodeAnim);
				}
			}

			// It's hard to decide whether or not the animation is "valid" for the given skeleton.
			// For example an animation does not necessarily contain channels for all bones.
			// Conversely, some channels in the animation might not be for bones.
			// So, you cannot simply check number of valid channels == number of bones
			// And you cannot simply check number of invalid channels == 0
			// For now, I've just decided on a simple number of valid channels must be at least 1
			return validChannels.size() > 0;
		}
	}

	u32 MeshSource::GetAnimationCount() const
	{
		// not m_Animations.size() here, because we may not have actually loaded the animations yet
		return static_cast<uint32_t>(m_Scene->mNumAnimations);
	}

	const Animation* MeshSource::GetAnimation(const u32 animationIndex, const Skeleton* skeleton) const
	{
		if (!m_Skeleton)
		{
			// No skeleton means this meshsource contains just animations (and no skin).  In this case
			// assimp does not import the bones and we will have no skeleton.
			// Copy the given skeleton to this mesh source, and then we can import the animations
			m_Skeleton = new Skeleton(*skeleton);
			const auto animationNames = GetAnimationNames(m_Scene);
			m_Animations.reserve(std::size(animationNames));
			for (const auto& animationName : animationNames)
			{
				m_Animations.emplace_back(ImportAnimation(m_Scene, animationName, m_Skeleton));
			}
		}
		assert(animationIndex < m_Animations.size() && "Animation index out of range!");
		assert(m_Animations[animationIndex] && "Attempted to access null animation!");
		return m_Animations[animationIndex];
	}

	void MeshSource::TraverseNodes(aiNode* node, const laml::Mat4& parentTransform, u32 level)
	{
		laml::Mat4 localTransform = utils::mat4_from_aiMatrix4x4(node->mTransformation);
		laml::Mat4 transform = laml::mul(parentTransform, localTransform);
		m_NodeMap[node].resize(node->mNumMeshes);
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			uint32_t mesh = node->mMeshes[i];
			auto& submesh = m_Submeshes[mesh];
			submesh.NodeName = node->mName.C_Str();
			submesh.Transform = transform;
			submesh.LocalTransform = localTransform;
			m_NodeMap[node][i] = mesh;
		}

		// HZ_MESH_LOG("{0} {1}", LevelToSpaces(level), node->mName.C_Str());

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
	}
}