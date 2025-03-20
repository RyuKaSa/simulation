#include "EquirectangularConverter.hpp"
#include <iostream>

EquirectangularConverter::EquirectangularConverter()
    : fbo(0)
    , rbo(0)
    , equirectTexID(0)
    , outWidth(512)
    , outHeight(256)
    , quadVAO(0)
    , quadVBO(0)
{
}

EquirectangularConverter::~EquirectangularConverter() {
    if (fbo)          glDeleteFramebuffers(1, &fbo);
    if (rbo)          glDeleteRenderbuffers(1, &rbo);
    if (equirectTexID) glDeleteTextures(1, &equirectTexID);
    if (quadVAO)      glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO)      glDeleteBuffers(1, &quadVBO);
}

void EquirectangularConverter::init(int width, int height) {
    outWidth = width;
    outHeight = height;

    // Load the equiShader from external files:
    if (!equiShader.load("src/shaders/equirect_convert.vs.glsl", "src/shaders/equirect_convert.fs.glsl")) {
        std::cerr << "Failed to load equirectangular conversion shaders.\n";
    }

    // Create FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create the 2D texture for equirect output
    glGenTextures(1, &equirectTexID);
    glBindTexture(GL_TEXTURE_2D, equirectTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outWidth, outHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           equirectTexID,
                           0);

    // Create a renderbuffer for depth, if needed
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, outWidth, outHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[EquirectangularConverter] Framebuffer not complete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Prepare fullscreen quad geometry
    initFullscreenQuad();
}

void EquirectangularConverter::initFullscreenQuad() {
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
                 quadVertices, GL_STATIC_DRAW);

    // Positions
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    // TexCoords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

unsigned int EquirectangularConverter::convert(unsigned int cubemapTexID) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, outWidth, outHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    equiShader.use();
    // Set sampler to the cubemap
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexID);
    equiShader.setUniform("uCubemap", 0);

    // Draw the quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return equirectTexID;
}