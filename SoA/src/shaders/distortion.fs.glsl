#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uEquirect;   // The 2D equirectangular map
uniform float uFactor;         // 0..1 distortion factor

void main()
{
    // Convert [0..1] -> [-1..1]
    vec2 uv = TexCoords * 2.0 - 1.0;
    float r = length(uv);
    // Interpolate warp strength: 1.0 means no distortion; lower values mean more curvature.
    float warpStrength = mix(1.0, 0.7, uFactor);
    float warpedR = pow(r, warpStrength);
    float theta = atan(uv.y, uv.x);
    vec2 warpedUV = warpedR * vec2(cos(theta), sin(theta));
    warpedUV = warpedUV * 0.5 + 0.5;
    vec2 eqUV = clamp(warpedUV, 0.0, 1.0);
    vec3 color = texture(uEquirect, eqUV).rgb;
    FragColor = vec4(color, 1.0);
}