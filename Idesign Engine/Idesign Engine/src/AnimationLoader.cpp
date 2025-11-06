#include "pch.h"
#include "AnimationLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

namespace Framework
{
    bool AnimationLoader::LoadAnimationData(const std::string& path,
        Framework::Skeleton& skeleton,
        std::map<std::string, Framework::Animation*>& animations)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);

        if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
            std::cerr << "ASSIMP error loading animation data: " << importer.GetErrorString() << std::endl;
            return false;
        }

        // 1. Load the skeleton hierarchy (parent/child relationships and default transforms)
        // We pass -1 as the parentIndex for the root node.
        LoadSkeletonHierarchy(scene->mRootNode, skeleton, -1);

        // 2. Load all animation clips
        LoadAnimationClips(scene, animations);

        return true;
    }

    void AnimationLoader::LoadSkeletonHierarchy(aiNode* node,
        Framework::Skeleton& skeleton,
        int parentIndex)
    {
        std::string nodeName = node->mName.C_Str();
        int currentBoneIndex = skeleton.GetBoneIndex(nodeName);

        // Check if this node is a bone that we've already registered in MeshLoader
        if (currentBoneIndex != -1)
        {
            // It is! Update its parent and default transform.
            Bone& bone = skeleton.bones[currentBoneIndex];
            bone.parentIndex = parentIndex;
            bone.defaultLocalTransform = ConvertAssimpMatrix(node->mTransformation);

            // Set the new parent index for all children
            parentIndex = currentBoneIndex;
        }
        else
        {
            // This node is not a bone (e.g., an empty transform node),
            // but its children might be. We still pass its parentIndex down.
        }

        // Recurse for all children
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            LoadSkeletonHierarchy(node->mChildren[i], skeleton, parentIndex);
        }
    }

    void AnimationLoader::LoadAnimationClips(const aiScene* scene,
        std::map<std::string, Framework::Animation*>& animations)
    {
        for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
        {
            aiAnimation* aiAnim = scene->mAnimations[i];
            Animation* newAnim = new Animation(); // Heap-allocate

            newAnim->name = aiAnim->mName.C_Str();
            newAnim->duration = (float)aiAnim->mDuration;
            newAnim->ticksPerSecond = (float)aiAnim->mTicksPerSecond;
            if (newAnim->ticksPerSecond == 0) {
                newAnim->ticksPerSecond = 24.0f; // Default fallback
            }

            // Load all bone channels (keyframes)
            for (unsigned int j = 0; j < aiAnim->mNumChannels; ++j)
            {
                aiNodeAnim* aiChannel = aiAnim->mChannels[j];
                std::string boneName = aiChannel->mNodeName.C_Str();

                BoneAnimation boneAnim;
                boneAnim.boneName = boneName;

                // Load Positions
                for (unsigned int k = 0; k < aiChannel->mNumPositionKeys; ++k)
                {
                    KeyframePosition key;
                    key.timeStamp = (float)aiChannel->mPositionKeys[k].mTime;
                    key.position.x = aiChannel->mPositionKeys[k].mValue.x;
                    key.position.y = aiChannel->mPositionKeys[k].mValue.y;
                    key.position.z = aiChannel->mPositionKeys[k].mValue.z;
                    boneAnim.positions.push_back(key);
                }

                // Load Rotations
                for (unsigned int k = 0; k < aiChannel->mNumRotationKeys; ++k)
                {
                    KeyframeRotation key;
                    key.timeStamp = (float)aiChannel->mRotationKeys[k].mTime;
                    key.rotation.w = aiChannel->mRotationKeys[k].mValue.w;
                    key.rotation.x = aiChannel->mRotationKeys[k].mValue.x;
                    key.rotation.y = aiChannel->mRotationKeys[k].mValue.y;
                    key.rotation.z = aiChannel->mRotationKeys[k].mValue.z;
                    boneAnim.rotations.push_back(key);
                }

                // Load Scales
                for (unsigned int k = 0; k < aiChannel->mNumScalingKeys; ++k)
                {
                    KeyframeScale key;
                    key.timeStamp = (float)aiChannel->mScalingKeys[k].mTime;
                    key.scale.x = aiChannel->mScalingKeys[k].mValue.x;
                    key.scale.y = aiChannel->mScalingKeys[k].mValue.y;
                    key.scale.z = aiChannel->mScalingKeys[k].mValue.z;
                    boneAnim.scales.push_back(key);
                }

                newAnim->boneChannels[boneName] = boneAnim;
            }

            // Store the new animation in the map
            animations[newAnim->name] = newAnim;
            std::cout << "Loaded animation clip: " << newAnim->name << std::endl;
        }
    }
}