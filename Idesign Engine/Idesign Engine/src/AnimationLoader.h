#pragma once

#include <string>
#include <map>
#include "Animation.h" // Your existing animation structs
#include <assimp/scene.h>

namespace Framework
{
    class AnimationLoader
    {
    public:
        // Loads skeleton hierarchy and all animation clips from a file
        static bool LoadAnimationData(const std::string& path,
            Framework::Skeleton& skeleton,
            std::map<std::string, Framework::Animation*>& animations);

    private:
        // Recursively processes the Assimp node tree to build our skeleton hierarchy
        static void LoadSkeletonHierarchy(aiNode* node,
            Framework::Skeleton& skeleton,
            int parentIndex);

        // Processes all aiAnimation objects in the scene
        static void LoadAnimationClips(const aiScene* scene,
            std::map<std::string, Framework::Animation*>& animations);
    };
}