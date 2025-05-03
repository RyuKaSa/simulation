#ifndef EQUIRECTANGULAR_CONVERTER_HPP
#define EQUIRECTANGULAR_CONVERTER_HPP

#include <glad/glad.h>
#include "Shader.hpp"

class EquirectangularConverter {
public:
    EquirectangularConverter();
    ~EquirectangularConverter();

    // Call once at startup or when resolution changes
    void init(int width, int height);

    // Convert the given cubemap into an equirectangular texture.
    // Returns the 2D texture ID containing the equirect map.
    // Also binds that texture as active at the end of the call.
    unsigned int convert(unsigned int cubemapTexID);

    unsigned int getEquirectTextureID() const { return equirectTexID; }

private:
    unsigned int fbo;
    unsigned int rbo;
    unsigned int equirectTexID;
    int outWidth, outHeight;
    unsigned int quadVAO, quadVBO;
    Shader equiShader;

    void initFullscreenQuad();
};

#endif // EQUIRECTANGULAR_CONVERTER_HPP