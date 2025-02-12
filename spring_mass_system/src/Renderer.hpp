#ifndef RENDERER_H
#define RENDERER_H

#include "Simulation.hpp"

class Renderer {
public:
    Renderer();
    ~Renderer();
    void render(const Simulation& simulation);
private:
    unsigned int shaderProgram;
    unsigned int vao, vbo;
    void init();
    void initShaders();
    int numSegments = 32;
};

#endif // RENDERER_H