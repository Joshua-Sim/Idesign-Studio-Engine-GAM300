// MeshLoader.h
#pragma once
#include <string>
#include "MeshProperties.h"
#include "AnimationProperties.h"



// Forward declarations for Assimp types
struct aiNode;
struct aiScene;

using namespace Framework;
class MeshLoader {

static void ProcessNode(aiNode* node, const aiScene* scene,
        Framework::Skeleton& skeleton, int parentBoneIndex);
public:
    static void loadMesh(const std::string& path, MeshProperties& mesh);

    /**
     * @brief Loads an ANIMATED mesh (with skeleton and bones).
     */
    static void loadMesh(const std::string& path,
        Framework::MeshProperties& mesh,
        Framework::AnimationProperties& animProps);
};
