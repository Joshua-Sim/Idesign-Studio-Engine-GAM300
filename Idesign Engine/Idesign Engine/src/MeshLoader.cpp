// MeshLoader.cpp
#include "pch.h"
#include "MeshLoader.h"
#include <glew.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>

#define MAX_BONES_PER_VERTEX 4

struct Vertex {
    float pos[3];
    float normal[3];
    float tex[2];
};

struct VertexSkinning
{
    // Indices of the bones that affect this vertex
    glm::ivec4 boneIDs{ -1, -1, -1, -1 };

    // The weight (influence) of each bone
    glm::vec4 weights{ 0.0f, 0.0f, 0.0f, 0.0f };

    void AddBoneData(int boneID, float weight)
    {
        for (int i = 0; i < MAX_BONES_PER_VERTEX; ++i)
        {
            if (boneIDs[i] == -1) 
            {
                boneIDs[i] = boneID;
                weights[i] = weight;
                return;
            }
        }
    }
};


static std::map<std::string, int> boneMapping;
static int boneCount = 0;


// Helper function to get or create a unique ID for a bone
int GetBoneID(const aiBone* bone)
{
    std::string boneName = bone->mName.C_Str();
    if (boneMapping.find(boneName) == boneMapping.end())
    {
        boneMapping[boneName] = boneCount;
        boneCount++;
    }
    return boneMapping[boneName];
}

void MeshLoader::loadMesh(const std::string& path, MeshProperties& mesh) {
    Assimp::Importer importer;

    // Triangulate faces, flip UVs if needed
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cerr << "ASSIMP error: " << importer.GetErrorString() << std::endl;
        return;
    }

    if (scene->mNumMeshes == 0) {
        std::cerr << "No meshes found in file: " << path << std::endl;
        return;
    }

    aiMesh* aimesh = scene->mMeshes[0]; // load only first mesh for now

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    std::vector<VertexSkinning> skinning_data(aimesh->mNumVertices);

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

    for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
        aiFace face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // --- NEW: Load Skinning Data (if it exists) ---
    if (aimesh->HasBones())
    {
        // Loop through each bone
        for (unsigned int i = 0; i < aimesh->mNumBones; ++i)
        {
            aiBone* bone = aimesh->mBones[i];
            int boneID = GetBoneID(bone); // Get or create a unique ID for this bone

            // Loop through each vertex this bone affects
            for (unsigned int j = 0; j < bone->mNumWeights; ++j)
            {
                const aiVertexWeight& weight = bone->mWeights[j];
                unsigned int vertexId = weight.mVertexId;
                float vertexWeight = weight.mWeight;

                // Add this bone's influence to the correct vertex
                if (vertexId < skinning_data.size())
                {
                    skinning_data[vertexId].AddBoneData(boneID, vertexWeight);
                }
            }
        }
    }
    // Update mesh path
    mesh.path = path;

    // OpenGL buffers
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);


    if (aimesh->HasBones())
    {
        glGenBuffers(1, &mesh.vbo_skinning); // Store this handle in MeshProperties
    }
    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    // texcoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));


    // --- NEW: VBO 2 (Skinning data: boneIDs, weights) ---
    if (aimesh->HasBones())
    {
        // Bind the new VBO
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_skinning);
        glBufferData(GL_ARRAY_BUFFER, skinning_data.size() * sizeof(VertexSkinning), skinning_data.data(), GL_STATIC_DRAW);

        // --- Attribute Pointers for VBO 2 ---

        // boneIDs (location = 3)
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer( // Use 'I'Pointer because boneIDs is ivec4 (integer)
            3, 4, GL_INT, sizeof(VertexSkinning), (void*)offsetof(VertexSkinning, boneIDs)
        );

        // weights (location = 4)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(
            4, 4, GL_FLOAT, GL_FALSE, sizeof(VertexSkinning), (void*)offsetof(VertexSkinning, weights)
        );
    }

    glBindVertexArray(0);

    mesh.indexCount = static_cast<unsigned int>(indices.size());

    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}
