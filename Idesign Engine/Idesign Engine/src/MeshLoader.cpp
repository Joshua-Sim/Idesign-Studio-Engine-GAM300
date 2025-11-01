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

// --- Helper Function ---
namespace Framework
{
    inline glm::mat4 ConvertAssimpMatrix(const aiMatrix4x4& from)
    {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }
}

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

    // --- FIX: OpenGL Buffers added for static mesh (prevents crash) ---
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

    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}


// --- HIERARCHY LOADER (NEW) ---
void MeshLoader::ProcessNode(aiNode* node, const aiScene* scene,
    Framework::Skeleton& skeleton, int parentBoneIndex)
{
    std::string nodeName = node->mName.C_Str();
    int currentBoneIndex = -1;

    // Check if this node is a bone we have already registered
    if (skeleton.boneNameMap.find(nodeName) != skeleton.boneNameMap.end())
    {
        // This node IS a bone
        currentBoneIndex = skeleton.boneNameMap[nodeName];
        Framework::Bone& bone = skeleton.bones[currentBoneIndex];

        // Set its parent and local transform
        bone.parentIndex = parentBoneIndex;
        bone.defaultLocalTransform = Framework::ConvertAssimpMatrix(node->mTransformation);

        // If this is the first bone we find with no parent, it's the root
        if (parentBoneIndex == -1) {
            skeleton.rootBoneIndex = currentBoneIndex;
        }
    }
    else
    {
        // This node is not a bone (e.g., just a transform group)
        // Pass the parent's index down to children
        currentBoneIndex = parentBoneIndex;
    }

    // Recurse for all children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, skeleton, currentBoneIndex);
    }
}


// --- ANIMATED MESH LOADER (FIXED) ---
void MeshLoader::loadMesh(const std::string& path,
    Framework::MeshProperties& mesh,
    Framework::AnimationProperties& animProps) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!scene || !scene->mRootNode || !scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cerr << "ASSIMP error: " << importer.GetErrorString() << std::endl;
        return;
    }
    if (scene->mNumMeshes == 0) {
        std::cerr << "No meshes found in file: " << path << std::endl;
        return;
    }

    aiMesh* aimesh = scene->mMeshes[0];
    std::vector<Vertex> vertices(aimesh->mNumVertices);
    std::vector<unsigned int> indices;
    std::vector<VertexSkinning> skinning_data(aimesh->mNumVertices);

    // Load Vertices (Position, Normal, UV)
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
                newBone.inverseBindPoseMatrix = Framework::ConvertAssimpMatrix(bone->mOffsetMatrix);
                newBone.boneID = static_cast<int>(animProps.skeleton.bones.size());

                animProps.skeleton.bones.push_back(newBone);
                animProps.skeleton.boneNameMap[boneName] = newBone.boneID;
            }
        }

        // Resize final matrices vector to match the *exact* bone count
        animProps.finalBoneMatrices.resize(animProps.skeleton.bones.size(), glm::mat4(1.0f));

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

    mesh.path = path;

    // --- OpenGL buffers ---
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);

    // VBO 1 (Vertices: Pos, Normal, UV)
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // VBO 1 Attrib Pointers
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); // tex
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));

    // VBO 2 (Skinning: Bone IDs, Weights)
    if (aimesh->HasBones())
    {
        glGenBuffers(1, &animProps.vbo_skinning); // Create and store VBO handle
        glBindBuffer(GL_ARRAY_BUFFER, animProps.vbo_skinning);
        glBufferData(GL_ARRAY_BUFFER, skinning_data.size() * sizeof(VertexSkinning), skinning_data.data(), GL_STATIC_DRAW);

        // VBO 2 Attrib Pointers
        glEnableVertexAttribArray(3); // boneIDs
        glVertexAttribIPointer( // Use 'I' for integers
            3, 4, GL_INT, sizeof(VertexSkinning), (void*)offsetof(VertexSkinning, boneIDs)
        );
        glEnableVertexAttribArray(4); // weights
        glVertexAttribPointer(
            4, 4, GL_FLOAT, GL_FALSE, sizeof(VertexSkinning), (void*)offsetof(VertexSkinning, weights)
        );
    }

    glBindVertexArray(0);
    mesh.indexCount = static_cast<unsigned int>(indices.size());
    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}