#pragma once

#include <string>
#include "MeshProperties.h"
#include "AnimationProperties.h"

// Forward declarations for Assimp types
struct aiNode;
struct aiScene;

class MeshLoader
{
private:
    /**
     * @brief Recursively processes the Assimp node hierarchy
     * to build the bone hierarchy (parent/child relationships).
     */
    static void ProcessNode(aiNode* node, const aiScene* scene,
        Framework::Skeleton& skeleton, int parentBoneIndex);

public:
    /**
     * @brief Loads a STATIC mesh (no bones).
     */
    static void loadMesh(const std::string& path, Framework::MeshProperties& mesh);

    /**
     * @brief Loads an ANIMATED mesh (with skeleton and bones).
     */
    static void loadMesh(const std::string& path,
        Framework::MeshProperties& mesh,
        Framework::AnimationProperties& animProps);
};