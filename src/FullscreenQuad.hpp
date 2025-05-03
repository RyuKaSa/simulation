#pragma once

#include <glad/glad.h>
#include "Shader.hpp"

class FullscreenQuad {
public:
    FullscreenQuad();
    ~FullscreenQuad();

    // Render the quad using the provided texture (from the camera FBO)
    void render(unsigned int textureID);

private:
    unsigned int quadVAO, quadVBO;
    Shader postprocessShader;

    void init();
};