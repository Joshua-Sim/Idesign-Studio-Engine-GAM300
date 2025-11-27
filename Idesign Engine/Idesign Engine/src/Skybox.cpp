#include "pch.h"
#include "Skybox.h"
#include <iostream>
// #define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h" 

Skybox::Skybox() {}

Skybox::~Skybox() {
    // 1. Cleanup Mesh Data
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);

    // 2. Cleanup Textures
    // ERROR WAS HERE: You must delete both the Day and Night textures now.
    // 'cubemapTexture' likely no longer exists or isn't used.
    if (texDay) glDeleteTextures(1, &texDay);
    if (texNight) glDeleteTextures(1, &texNight);

    // 3. Cleanup Shader
    if (shader) {
        shader->destroy(); // delete the OpenGL program ID
        delete shader;     // delete the C++ pointer
        shader = nullptr;  // safety
    }
}

void Skybox::init(const std::vector<std::string>& dayFaces,
    const std::vector<std::string>& nightFaces)
{
    // 1. Compile Shader
    shader = new ShaderProgram("Assets/shaders/skybox_vert.glsl", "Assets/shaders/skybox_frag.glsl");
    shader->initGL();

    // 2. Define Skybox Vertices
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // 3. Init Buffers
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0); // Unbind VAO

    // 3. Load BOTH textures
    texDay = loadCubemap(dayFaces);
    texNight = loadCubemap(nightFaces);

    // 4. Configure Shader Samplers
    shader->use();
    shader->setInt("skyboxDay", 0);   // Texture Unit 0
    shader->setInt("skyboxNight", 1); // Texture Unit 1
    glUseProgram(0);
}

void Skybox::draw(const glm::mat4& view, const glm::mat4& proj)
{

    glDepthFunc(GL_LEQUAL);

    shader->use();
    shader->setMat4("uView", view);
    shader->setMat4("uProj", proj);
    shader->setFloat("blendFactor", m_blendFactor); // Send the blend factor

    glBindVertexArray(cubeVAO);

    // Bind Day to Unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texDay);

    // Bind Night to Unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texNight);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Cleanup
    glBindVertexArray(0);

    // Unbind both to be safe
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glDepthFunc(GL_LESS);
}

unsigned int Skybox::loadCubemap(const std::vector<std::string>& faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // Skyboxes should generally NOT be flipped. 
    // We save the state if we want to be polite to other systems, or just force false.
    stbi_set_flip_vertically_on_load(false);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Reset flip to true because your TextureLoader expects it to be true for normal PNGs
    stbi_set_flip_vertically_on_load(true);

    // CRITICAL FIX: Unbind the texture so it doesn't pollute the state machine
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return textureID;
}