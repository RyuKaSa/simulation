#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform samplerCube uCubemap;

// We assume -1..1 in TexCoords, or 0..1. We'll adapt in code:
void main()
{
    // Convert [0..1] -> [-1..1]
    vec2 uv = TexCoords * 2.0 - 1.0;

    // phi   = uv.x * PI   (horizontal,  -1..1 -> -PI..PI)
    // theta = uv.y * PI/2 (vertical,    -1..1 -> -PI/2..PI/2)
    float phi   = uv.x * 3.14159;      // -π..π
    float theta = uv.y * 1.570795;     // -π/2..π/2

    // Spherical to cartesian
    float x = cos(theta) * cos(phi);
    float y = sin(theta);
    float z = cos(theta) * sin(phi);

    vec3 dir = normalize(vec3(x, y, z));
    vec3 color = texture(uCubemap, dir).rgb;

    FragColor = vec4(color, 1.0);
}