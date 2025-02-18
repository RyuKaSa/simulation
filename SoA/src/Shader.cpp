#include "Shader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>

Shader::Shader() : programID(0) {}

Shader::~Shader() {
    if(programID)
        glDeleteProgram(programID);
}

std::string Shader::readFile(const std::string &filePath) {
    std::ifstream fileStream(filePath);
    if(!fileStream) {
        std::cerr << "ERROR: Could not open file " << filePath << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << fileStream.rdbuf();
    return ss.str();
}

bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexCode = readFile(vertexPath);
    std::string fragmentCode = readFile(fragmentPath);
    if(vertexCode.empty() || fragmentCode.empty())
        return false;

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);
    if(!checkCompileErrors(vertex, "VERTEX"))
        return false;

    // Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);
    if(!checkCompileErrors(fragment, "FRAGMENT"))
        return false;

    // Shader Program
    programID = glCreateProgram();
    glAttachShader(programID, vertex);
    glAttachShader(programID, fragment);
    glLinkProgram(programID);
    // Check linking errors
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(programID, 512, nullptr, infoLog);
        std::cerr << "ERROR: Shader Program Linking Failed: " << infoLog << std::endl;
        return false;
    }

    // Clean up shaders (they’re linked now)
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return true;
}

bool Shader::checkCompileErrors(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[512];
    if(type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "ERROR: " << type << " Shader Compilation Failed: " << infoLog << std::endl;
            return false;
        }
    }
    return true;
}

void Shader::use() const {
    glUseProgram(programID);
}

void Shader::setUniform(const std::string &name, int value) const {
    glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
}

void Shader::setUniform(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
}

void Shader::setUniform(const std::string &name, const glm::vec3 &value) const {
    glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
}

void Shader::setUniform(const std::string &name, const glm::mat4 &value) const {
    glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &value[0][0]);
}