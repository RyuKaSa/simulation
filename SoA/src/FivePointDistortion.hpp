#ifndef FIVE_POINT_DISTORTION_HPP
#define FIVE_POINT_DISTORTION_HPP

#include <glad/glad.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include "Shader.hpp"

class FivePointDistortion {
public:
    FivePointDistortion();
    ~FivePointDistortion();

    void init(int width, int height);

    // Distort the equirectangular texture onto the screen.
    // 'factor' is [0..1], where 0 = no distortion, 1 = full 5-point style.
    void render(unsigned int equirectTexID, float factor, const glm::mat4& viewMatrix);

    void setShowCrosshair(bool value) { showCrosshair = value; }

private:
    unsigned int quadVAO, quadVBO;
    Shader distortionShader;

    bool showCrosshair;

    void initFullscreenQuad();
};

#endif // FIVE_POINT_DISTORTION_HPP