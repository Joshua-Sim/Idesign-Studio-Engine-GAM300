#pragma once
#include <string>
#include <glew.h>
#include <glm.hpp>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ShaderProgram(const std::string& vpath, const std::string& fpath);

    ~ShaderProgram();

    // Upload to GPU
    void initGL();

    void use() const;

    // uniform helpers
    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setFloat(const char* name, float f) const;
    void setInt(const char* name, int i) const;
    void setBool(const char* name, bool b) const;

    // convenience for lights
    void setDirLight(const glm::vec3& dir, const glm::vec3& color, float intensity);
    void setPointLight(int idx, const glm::vec3& pos, const glm::vec3& color, float intensity, float radius);

    void destroy();

private:
    std::string vsrc;
    std::string fsrc;

    GLuint ID = 0;

    static std::string loadShaderSource(const std::string& path);
    static GLuint compile(GLenum type, const std::string& src);
};