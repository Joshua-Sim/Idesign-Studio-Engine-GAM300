#include "pch.h"
#include "ShaderProgram.h"
#include <fstream>
#include <sstream>
#include <iostream>

ShaderProgram::ShaderProgram(const std::string& vpath, const std::string& fpath) {
    vsrc = loadShaderSource(vpath);
    fsrc = loadShaderSource(fpath);
}

ShaderProgram::~ShaderProgram() {
    if (ID) glDeleteProgram(ID);
}

void ShaderProgram::initGL() {
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc);

    ID = glCreateProgram();
    glAttachShader(ID, vs);
    glAttachShader(ID, fs);
    glLinkProgram(ID);

    GLint ok;
    glGetProgramiv(ID, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(ID, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

void ShaderProgram::use() const {
    glUseProgram(ID);
}

void ShaderProgram::setMat4(const char* name, const glm::mat4& m) const {
    GLint loc = glGetUniformLocation(ID, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}

void ShaderProgram::setVec3(const char* name, const glm::vec3& v) const {
    GLint loc = glGetUniformLocation(ID, name);
    glUniform3fv(loc, 1, &v[0]);
}

void ShaderProgram::setFloat(const char* name, float f) const {
    GLint loc = glGetUniformLocation(ID, name);
    glUniform1f(loc, f);
}

void ShaderProgram::setInt(const char* name, int i) const {
    GLint loc = glGetUniformLocation(ID, name);
    glUniform1i(loc, i);
}

void ShaderProgram::setBool(const char* name, bool b) const {
    GLint loc = glGetUniformLocation(ID, name);
    glUniform1i(loc, b ? 1 : 0);
}

void ShaderProgram::setDirLight(const glm::vec3& dir, const glm::vec3& color, float intensity) {
    setVec3("uDirLight.direction", dir);
    setVec3("uDirLight.color", color);
    setFloat("uDirLight.intensity", intensity);
}

void ShaderProgram::setPointLight(int idx, const glm::vec3& pos, const glm::vec3& color, float intensity, float radius) {
    std::string base = "uPointLights[" + std::to_string(idx) + "]";
    setVec3((base + ".position").c_str(), pos);
    setVec3((base + ".color").c_str(), color);
    setFloat((base + ".intensity").c_str(), intensity);
    setFloat((base + ".radius").c_str(), radius);
}

std::string ShaderProgram::loadShaderSource(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open shader: " << path << "\n";
        return {};
    }
    std::stringstream ss; ss << ifs.rdbuf();
    return ss.str();
}

GLuint ShaderProgram::compile(GLenum type, const std::string& src) {
    GLuint sh = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);

    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, 1024, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
    }
    return sh;
}