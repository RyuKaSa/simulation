#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uEquirect;  // The equirectangular environment map
uniform mat4 viewMatrix;      // The camera view matrix

const float PI = 3.14159265359;

void main() {
    // Convert TexCoords from [0,1] to [-1,1] (Normalized Device Coordinates)
    vec2 uv = TexCoords * 2.0 - 1.0;

    // Define the FOV for perspective projection
    float fov = radians(90.0);  // Set to 90 degrees FOV
    float tanHalfFOV = tan(fov * 0.5);

    // Compute direction in camera space
    vec3 dir = normalize(vec3(uv.x * tanHalfFOV, uv.y * tanHalfFOV, -1.0)); // Use -1.0 for correct forward direction

    // Extract only rotation from view matrix
    mat3 rotationMatrix = mat3(viewMatrix);

    // Apply rotation to direction
    dir = normalize(rotationMatrix * dir);

    // Flip Y to match OpenGL's coordinate system
    dir.y = -dir.y;

    // Convert world space direction to spherical coordinates
    float lon = atan(dir.x, dir.z);   // Longitude (-π to π) → Swap X and Z
    float lat = asin(dir.y);          // Latitude (-π/2 to π/2)

    // Convert spherical coordinates to equirectangular UVs
    float u = (lon / (2.0 * PI)) + 0.5;
    float v = (lat / PI) + 0.5;

    // Sample the equirectangular environment map
    vec3 color = texture(uEquirect, vec2(u, v)).rgb;

    FragColor = vec4(color, 1.0);
}