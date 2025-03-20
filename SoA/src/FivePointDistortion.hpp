#ifndef FIVE_POINT_DISTORTION_HPP
#define FIVE_POINT_DISTORTION_HPP

#include <glad/glad.h>
#include "Shader.hpp"

class FivePointDistortion {
public:
    FivePointDistortion();
    ~FivePointDistortion();

    void init(int width, int height);

    // Distort the equirectangular texture onto the screen.
    // 'factor' is [0..1], where 0 = no distortion, 1 = full 5-point style.
    void render(unsigned int equirectTexID, float factor);

private:
    unsigned int quadVAO, quadVBO;
    Shader distortionShader;

    void initFullscreenQuad();
};

#endif // FIVE_POINT_DISTORTION_HPP