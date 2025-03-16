#include "FullscreenQuad.hpp"
#include <iostream>

// Vertex data for a fullscreen quad using a triangle strip.
// Each vertex has a 2D position and a 2D texture coordinate.
static const float quadVertices[] = {
    // positions   // texCoords
    -1.0f,  1.0f,  0.0f, 1.0f, // top-left
    -1.0f, -1.0f,  0.0f, 0.0f, // bottom-left
     1.0f,  1.0f,  1.0f, 1.0f, // top-right
     1.0f, -1.0f,  1.0f, 0.0f  // bottom-right
};

FullscreenQuad::FullscreenQuad() {
    init();
}

FullscreenQuad::~FullscreenQuad() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
}

void FullscreenQuad::init() {
    // Load the post-processing shaders.
    // Place these shader files in your "src/shaders/" folder.
    if (!postprocessShader.load("src/shaders/postprocess.vs.glsl", "src/shaders/postprocess.fs.glsl")) {
        std::cerr << "Failed to load postprocess shaders." << std::endl;
    }

    // Generate and bind the VAO and VBO for the quad.
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Attribute 0: position (2 floats)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Attribute 1: texture coordinates (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void FullscreenQuad::render(unsigned int textureID) {
    // Disable depth testing so the quad appears on top.
    glDisable(GL_DEPTH_TEST);

    postprocessShader.use();
    // Bind the FBO's color texture.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    postprocessShader.setUniform("uTexture", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Re-enable depth testing if needed later.
    glEnable(GL_DEPTH_TEST);
}