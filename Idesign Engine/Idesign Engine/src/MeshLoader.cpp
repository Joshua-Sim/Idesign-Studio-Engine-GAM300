#include "pch.h"
#include "MeshLoader.h"
#include <glew.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>

#include <glm.hpp>
#include <assimp/matrix4x4.h>
#include "AnimationProperties.h" // Includes Skeleton.h

#define MAX_BONES_PER_VERTEX 4

// --- Helper Structs ---
struct Vertex {
    float pos[3];
    float normal[3];
    float tex[2];
};


struct AnimatedVertex {
    float pos[3];
    float normal[3];
    float tex[2];
    glm::ivec4 boneIDs{ -1, -1, -1, -1 };
    glm::vec4 weights{ 0.0f, 0.0f, 0.0f, 0.0f };
};

struct VertexSkinning
{
    glm::ivec4 boneIDs{ -1, -1, -1, -1 };
    glm::vec4 weights{ 0.0f, 0.0f, 0.0f, 0.0f };

    void AddBoneData(int boneID, float weight) {
        for (int i = 0; i < MAX_BONES_PER_VERTEX; ++i) {
            if (boneIDs[i] == -1) {
                boneIDs[i] = boneID;
                weights[i] = weight;
                return;
            }
        }
    }
};

void MeshLoader::loadMesh(const std::string& path, Framework::MeshProperties& mesh) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cerr << "ASSIMP error: " << importer.GetErrorString() << std::endl;
        return;
    }
    if (scene->mNumMeshes == 0) {
        std::cerr << "No meshes found in file: " << path << std::endl;
        return;
    }

    aiMesh* aimesh = scene->mMeshes[0];

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // (Vertex data loading...)
    for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
        Vertex v{};
        v.pos[0] = aimesh->mVertices[i].x;
        v.pos[1] = aimesh->mVertices[i].y;
        v.pos[2] = aimesh->mVertices[i].z;
        if (aimesh->HasNormals()) {
            v.normal[0] = aimesh->mNormals[i].x;
            v.normal[1] = aimesh->mNormals[i].y;
            v.normal[2] = aimesh->mNormals[i].z;
        }
        if (aimesh->HasTextureCoords(0)) {
            v.tex[0] = aimesh->mTextureCoords[0][i].x;
            v.tex[1] = aimesh->mTextureCoords[0][i].y;
        }
        else {
            v.tex[0] = v.tex[1] = 0.0f;
        }
        vertices.push_back(v);
    }

    // (Index data loading...)
    for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
        aiFace face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // OpenGL Buffers added for static mesh (prevents crash) ---
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Vertex Attrib Pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
    mesh.indexCount = static_cast<unsigned int>(indices.size());


   // Store geometry for navmesh or other CPU use
    mesh.positions.clear();
    mesh.indices.clear();
    for (const auto& v : vertices) {
        mesh.positions.emplace_back(v.pos[0], v.pos[1], v.pos[2]);
    }
    mesh.indices.assign(indices.begin(), indices.end());

    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}


// --- HIERARCHY LOADER --
void MeshLoader::ProcessNode(aiNode* node, const aiScene* scene,
    Framework::Skeleton& skeleton, int parentBoneIndex)
{
    std::string nodeName = node->mName.C_Str();
    int currentBoneIndex = -1; // Default to "not a bone"

    // Check if this node is a bone we have already registered
    if (skeleton.boneNameMap.find(nodeName) != skeleton.boneNameMap.end())
    {
        // This node IS a bone
        currentBoneIndex = skeleton.boneNameMap[nodeName];
        Framework::Bone& bone = skeleton.bones[currentBoneIndex];

        // Set its parent and local transform
        bone.parentIndex = parentBoneIndex;
        bone.defaultLocalTransform = ConvertAssimpMatrix(node->mTransformation);

        // --- THIS IS THE MISSING LOGIC ---
        // If this bone has no parent, it's a root bone
        if (parentBoneIndex == -1)
        {
            skeleton.rootBoneIndex = currentBoneIndex;
        }
        // --- END OF MISSING LOGIC ---

        // Add this bone as a child of its parent (for the animation system)
        if (parentBoneIndex != -1)
        {
            skeleton.bones[parentBoneIndex].childrenIndices.push_back(currentBoneIndex);
        }
    }
    else
    {
        // This node is not a bone (e.g., "Armature" or a transform group)
        // Pass the parent's index down to children
        currentBoneIndex = parentBoneIndex;
    }

    // Recurse for all children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, skeleton, currentBoneIndex);
    }
}


// --- ANIMATED MESH LOADER---
void MeshLoader::loadMesh(const std::string& path,
    Framework::MeshProperties& mesh,
    Framework::AnimationProperties& animProps) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );



    if (scene->mNumMeshes == 0) {
        std::cerr << "No meshes found in file: " << path << std::endl;
        return;
    }

    aiMesh* aimesh = nullptr;
    std::cout << "Searching " << scene->mNumMeshes << " meshes for a renderable mesh..." << std::endl;
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* tempMesh = scene->mMeshes[i];
        std::cout << "  - Checking mesh " << i << ": "
            << tempMesh->mName.C_Str()
            << " (Vertices: " << tempMesh->mNumVertices
            << ", Bones: " << tempMesh->mNumBones << ")" << std::endl;

        // We need the mesh that has both vertices AND bones
        if (tempMesh->mNumVertices > 0 && tempMesh->HasBones())
        {
            aimesh = tempMesh;
            std::cout << "  >> Found renderable mesh at index: " << i << std::endl;
            break; // Stop searching
        }
    }

    // If no suitable mesh was found, error out
    if (aimesh == nullptr)
    {
        std::cerr << "ASSIMP error: No mesh with both vertices and bones found in file: " << path << std::endl;
        return;
    }

    std::vector<AnimatedVertex> vertices(aimesh->mNumVertices);
    std::vector<unsigned int> indices;
    std::vector<VertexSkinning> skinning_data(aimesh->mNumVertices);

    for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
        vertices[i].pos[0] = aimesh->mVertices[i].x;
        vertices[i].pos[1] = aimesh->mVertices[i].y;
        vertices[i].pos[2] = aimesh->mVertices[i].z;

        if (aimesh->HasNormals()) {
            vertices[i].normal[0] = aimesh->mNormals[i].x;
            vertices[i].normal[1] = aimesh->mNormals[i].y;
            vertices[i].normal[2] = aimesh->mNormals[i].z;
        }

        if (aimesh->HasTextureCoords(0)) {
            vertices[i].tex[0] = aimesh->mTextureCoords[0][i].x;
            vertices[i].tex[1] = aimesh->mTextureCoords[0][i].y;
        }
        else {
            vertices[i].tex[0] = vertices[i].tex[1] = 0.0f;
        }
    }
    // Load Indices
    for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
        aiFace face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // --- Load Skinning Data ---
    if (aimesh->HasBones())
    {
        // PASS 1: Read aiBone data (Name, ID, Inverse Bind Matrix)
        for (unsigned int i = 0; i < aimesh->mNumBones; ++i)
        {
            aiBone* bone = aimesh->mBones[i];
            std::string boneName = bone->mName.C_Str();

            if (animProps.skeleton.boneNameMap.find(boneName) == animProps.skeleton.boneNameMap.end())
            {
                Framework::Bone newBone;
                newBone.name = boneName;
                newBone.inverseBindPoseMatrix = ConvertAssimpMatrix(bone->mOffsetMatrix);
                newBone.boneID = static_cast<int>(animProps.skeleton.bones.size());

                animProps.skeleton.bones.push_back(newBone);
                animProps.skeleton.boneNameMap[boneName] = newBone.boneID;
            }
        }

        // Resize final matrices vector to match the *exact* bone count
        animProps.finalBoneMatrices.resize(animProps.skeleton.bones.size(), glm::mat4(1.0f));


        animProps.skeleton.rootNodeTransform = ConvertAssimpMatrix(scene->mRootNode->mTransformation);
        // PASS 2: Read aiNode hierarchy (Parenting, Local Transforms)
        // We start from the root node, passing -1 as the parent index
        ProcessNode(scene->mRootNode, scene, animProps.skeleton, -1);

        // PASS 3: Assign vertex weights
        for (unsigned int i = 0; i < aimesh->mNumBones; ++i)
        {
            aiBone* bone = aimesh->mBones[i];
            int boneID = animProps.skeleton.GetBoneIndex(bone->mName.C_Str());
            if (boneID == -1) continue;

            for (unsigned int j = 0; j < bone->mNumWeights; ++j)
            {
                const aiVertexWeight& weight = bone->mWeights[j];
                skinning_data[weight.mVertexId].AddBoneData(boneID, weight.mWeight);
            }
        }
    }

    for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
        vertices[i].boneIDs = skinning_data[i].boneIDs;
        vertices[i].weights = skinning_data[i].weights;
    }

    if (scene->HasAnimations())
    {
        LoadAnimations(scene, animProps);
    }
    mesh.path = path;


    // --- OpenGL buffers ---
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);

    // VBO 1 (Vertices: Pos, Normal, UV, Bones, Weights)
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    // --- CHANGE THIS LINE ---
    // glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW); // <-- DELETE THIS
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(AnimatedVertex), vertices.data(), GL_STATIC_DRAW); // <-- REPLACE WITH THIS
    // ----------------------

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // --- VBO 1 Attrib Pointers (ALL FROM ONE VBO) ---
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex), (void*)offsetof(AnimatedVertex, pos));

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex), (void*)offsetof(AnimatedVertex, normal));

    glEnableVertexAttribArray(2); // tex
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex), (void*)offsetof(AnimatedVertex, tex));

    // VBO 2 (Skinning: Bone IDs, Weights)
    // --- DELETE THIS ENTIRE BLOCK ---
    /*
    if (aimesh->HasBones())
    {
        glGenBuffers(1, &animProps.vbo_skinning);
        glBindBuffer(GL_ARRAY_BUFFER, animProps.vbo_skinning);
        glBufferData(GL_ARRAY_BUFFER, skinning_data.size() * sizeof(VertexSkinning), skinning_data.data(), GL_STATIC_DRAW);
    */
    // --------------------------------

    // --- REPLACE WITH NEW ATTRIBS (pointing to the SAME VBO) ---
    glEnableVertexAttribArray(3); // boneIDs
    glVertexAttribIPointer( // Use 'I' for integers
        3, 4, GL_INT, sizeof(AnimatedVertex), (void*)offsetof(AnimatedVertex, boneIDs)
    );

    glEnableVertexAttribArray(4); // weights
    glVertexAttribPointer(
        4, 4, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex), (void*)offsetof(AnimatedVertex, weights)
    );

    glBindVertexArray(0);
    mesh.indexCount = static_cast<unsigned int>(indices.size());

    // --- ADD THIS DEBUG BLOCK ---
    std::cout << "--- MESH LOADER STATS (FINAL) ---" << std::endl;
    std::cout << "  Vertices loaded: " << vertices.size() << std::endl;
    std::cout << "  Indices loaded: " << mesh.indexCount << std::endl;
    std::cout << "  Total meshes in file: " << scene->mNumMeshes << std::endl;
    std::cout << "  Root bone index: " << animProps.skeleton.rootBoneIndex << std::endl; // <-- THIS IS VITAL
    std::cout << "---------------------------------" << std::endl;
    // --- END OF DEBUG BLOCK ---
    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}


/**
 * @brief Loads all animation clips from the Assimp scene
 * and stores them in the AnimationProperties component.
 */
 /**
  * @brief Loads all animation clips from the Assimp scene
  * and stores them in the AnimationProperties component.
  */
void MeshLoader::LoadAnimations(const aiScene* scene, Framework::AnimationProperties& animProps)
{
    std::cout << "Loading " << scene->mNumAnimations << " animation(s)..." << std::endl;

    for (unsigned int i = 0; i < scene->mNumAnimations; i++)
    {
        aiAnimation* assimpAnim = scene->mAnimations[i];

        // We must heap-allocate the new animation,
        // as AnimationProperties stores it as a pointer
        Framework::Animation* newAnim = new Framework::Animation();

        newAnim->name = assimpAnim->mName.C_Str();
        if (newAnim->name.empty()) {
            newAnim->name = "Animation_" + std::to_string(i);
        }

        newAnim->ticksPerSecond = assimpAnim->mTicksPerSecond > 0 ?
            assimpAnim->mTicksPerSecond : 24.0f;
        newAnim->duration = assimpAnim->mDuration;


        // Loop through all bone channels in this animation
        for (unsigned int j = 0; j < assimpAnim->mNumChannels; j++)
        {
            aiNodeAnim* channel = assimpAnim->mChannels[j];
            std::string boneName = channel->mNodeName.C_Str();

            // Only load data for bones that are actually in our skeleton
            if (animProps.skeleton.boneNameMap.find(boneName) == animProps.skeleton.boneNameMap.end())
            {
                continue;
            }

            // ---!!!--- NEW DEBUG LINE ---!!!---
           /* std::cout << "  - Found Channel: [" << boneName << "]"
                << " (P: " << channel->mNumPositionKeys
                << ", R: " << channel->mNumRotationKeys
                << ", S: " << channel->mNumScalingKeys << ")" << std::endl;*/
            // ---!!!--- END OF NEW DEBUG LINE ---!!!---


            Framework::BoneAnimation boneAnim;
            boneAnim.boneName = boneName;

            // Load Position Keyframes
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++)
            {
                aiVector3D pos = channel->mPositionKeys[k].mValue;
                float time = channel->mPositionKeys[k].mTime;
                boneAnim.positions.push_back({ time, {pos.x, pos.y, pos.z} });
            }

            // Load Rotation Keyframes
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++)
            {
                aiQuaternion rot = channel->mRotationKeys[k].mValue;
                float time = channel->mRotationKeys[k].mTime;
                boneAnim.rotations.push_back({ time, {rot.w, rot.x, rot.y, rot.z} });
            }

            // Load Scale Keyframes
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++)
            {
                aiVector3D scale = channel->mScalingKeys[k].mValue;
                float time = channel->mScalingKeys[k].mTime;
                boneAnim.scales.push_back({ time, {scale.x, scale.y, scale.z} });
            }

            newAnim->boneChannels[boneName] = boneAnim;
        }

        // Store the new animation in the component's map
        animProps.availableAnimations[newAnim->name] = newAnim;
        std::cout << "  Loaded clip: " << newAnim->name << std::endl;
    }

    // Set the first animation as the default
    if (!animProps.availableAnimations.empty())
    {
        animProps.currentAnimation = animProps.availableAnimations.begin()->second;
        std::cout << "Set default animation to: " << animProps.currentAnimation->name << std::endl;
    }
}
