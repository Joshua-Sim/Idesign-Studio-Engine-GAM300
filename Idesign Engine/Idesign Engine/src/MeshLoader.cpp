// MeshLoader.cpp
#include "pch.h"
#include "MeshLoader.h"
#include <glew.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>

struct Vertex {
    float pos[3];
    float normal[3];
    float tex[2];
};

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
    
    // Update mesh path
    mesh.path = path;

    // OpenGL buffers
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

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

    glBindVertexArray(0);

    mesh.indexCount = static_cast<unsigned int>(indices.size());

    std::cout << mesh.guid << " " << mesh.path << " was loaded into system" << std::endl;
}
