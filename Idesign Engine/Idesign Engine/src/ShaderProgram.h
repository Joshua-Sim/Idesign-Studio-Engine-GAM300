#pragma once
#include <string>
#include <glew.h>
#include <glm.hpp>

class ShaderProgram {
public:
    GLuint ID = 0;

    ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    ~ShaderProgram();

    void use() const;

    // Uniform helpers
    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setFloat(const char* name, float f) const;
    void setInt(const char* name, int i) const;
    void setBool(const char* name, bool b) const;

    void setDirLight(const glm::vec3& dir, const glm::vec3& color, float intensity);
    void setPointLight(int idx, const glm::vec3& pos, const glm::vec3& color, float intensity, float radius);

private:
    static std::string loadShaderSource(const std::string& path);
    static GLuint compile(GLenum type, const std::string& source);
};
