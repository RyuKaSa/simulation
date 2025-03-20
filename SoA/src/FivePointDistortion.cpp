#include "FivePointDistortion.hpp"
#include <iostream>

FivePointDistortion::FivePointDistortion()
    : quadVAO(0)
    , quadVBO(0)
{}

FivePointDistortion::~FivePointDistortion() {
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void FivePointDistortion::init(int width, int height) {
    // Load the shader from files using your existing Shader::load:
    if (!distortionShader.load("src/shaders/distortion.vs.glsl", "src/shaders/distortion.fs.glsl")) {
        std::cerr << "Failed to load five-point distortion shaders.\n";
    }
    initFullscreenQuad();
}

void FivePointDistortion::initFullscreenQuad() {
    float quadVertices[16] = {
        // positions    // texCoords
        -1.f,  1.f,     0.f, 1.f,
        -1.f, -1.f,     0.f, 0.f,
         1.f,  1.f,     1.f, 1.f,
         1.f, -1.f,     1.f, 0.f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Positions
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // TexCoords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void FivePointDistortion::render(unsigned int equirectTexID, float factor) {
    glDisable(GL_DEPTH_TEST);

    distortionShader.use();
    distortionShader.setUniform("uFactor", factor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, equirectTexID);
    distortionShader.setUniform("uEquirect", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}