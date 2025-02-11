#include "glimac/Program.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>

namespace glimac {

bool Program::link() {
    glLinkProgram(m_nGLId);
    GLint status;
    glGetProgramiv(m_nGLId, GL_LINK_STATUS, &status);
    return status == GL_TRUE;
}

const std::string Program::getInfoLog() const {
    GLint length;
    glGetProgramiv(m_nGLId, GL_INFO_LOG_LENGTH, &length);
    char* log = new char[length];
    glGetProgramInfoLog(m_nGLId, length, 0, log);
    std::string logString(log);
    delete[] log;
    return logString;
}

// Build a GLSL program from source code
Program buildProgram(const GLchar* vsSrc, const GLchar* fsSrc) {
    Shader vs(GL_VERTEX_SHADER);
    vs.setSource(vsSrc);

    if (!vs.compile()) {
        throw std::runtime_error("Compilation error for vertex shader: " + vs.getInfoLog());
    }

    Shader fs(GL_FRAGMENT_SHADER);
    fs.setSource(fsSrc);

    if (!fs.compile()) {
        throw std::runtime_error("Compilation error for fragment shader: " + fs.getInfoLog());
    }

    Program program;
    program.attachShader(vs);
    program.attachShader(fs);

    if (!program.link()) {
        throw std::runtime_error("Link error: " + program.getInfoLog());
    }

    return program;
}

// Load source code from files and build a GLSL program (Vertex + Fragment)
Program loadProgram(const FilePath& vsFile, const FilePath& fsFile) {
    Shader vs = loadShader(GL_VERTEX_SHADER, vsFile);
    Shader fs = loadShader(GL_FRAGMENT_SHADER, fsFile);

    if (!vs.compile()) {
        throw std::runtime_error("Compilation error for vertex shader (from file " + vsFile.str() + "): " + vs.getInfoLog());
    }

    if (!fs.compile()) {
        throw std::runtime_error("Compilation error for fragment shader (from file " + fsFile.str() + "): " + fs.getInfoLog());
    }

    Program program;
    program.attachShader(vs);
    program.attachShader(fs);

    if (!program.link()) {
        throw std::runtime_error("Link error (for files " + vsFile.str() + " and " + fsFile.str() + "): " + program.getInfoLog());
    }

    return program;
}

// Load source code from files and build a GLSL program (Vertex + Geometry + Fragment)
Program loadProgram(const FilePath& vsFile, const FilePath& fsFile, const FilePath& gsFile) {
    Shader vs = loadShader(GL_VERTEX_SHADER, vsFile);
    Shader fs = loadShader(GL_FRAGMENT_SHADER, fsFile);
    Shader gs = loadShader(GL_GEOMETRY_SHADER, gsFile);

    if (!vs.compile()) {
        throw std::runtime_error("Compilation error for vertex shader (from file " + vsFile.str() + "): " + vs.getInfoLog());
    }

    if (!gs.compile()) {
        throw std::runtime_error("Compilation error for geometry shader (from file " + gsFile.str() + "): " + gs.getInfoLog());
    }

    if (!fs.compile()) {
        throw std::runtime_error("Compilation error for fragment shader (from file " + fsFile.str() + "): " + fs.getInfoLog());
    }

    Program program;
    program.attachShader(vs);
    program.attachShader(gs);
    program.attachShader(fs);

    if (!program.link()) {
        throw std::runtime_error("Link error (for files " + vsFile.str() + ", " + gsFile.str() + ", and " + fsFile.str() + "): " + program.getInfoLog());
    }

    return program;
}

} // namespace glimac