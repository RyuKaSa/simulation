#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader();
    ~Shader();

    // Load and compile vertex and fragment shaders from file paths.
    bool load(const std::string& vertexPath, const std::string& fragmentPath);

    // Use (activate) this shader program.
    void use() const;

    // Utility uniform setters.
    void setUniform(const std::string &name, int value) const;
    void setUniform(const std::string &name, float value) const;
    void setUniform(const std::string &name, const glm::vec3 &value) const;
    void setUniform(const std::string &name, const glm::mat4 &value) const;

    // Return the program ID.
    unsigned int getID() const { return programID; }

private:
    unsigned int programID;

    std::string readFile(const std::string &filePath);
    bool checkCompileErrors(unsigned int shader, const std::string& type);
};

#endif // SHADER_HPP