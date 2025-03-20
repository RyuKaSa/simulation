#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uEquirect;  // The equirectangular texture
uniform mat4 viewMatrix;      // The FPS camera's view matrix (world-to-camera)

const float PI = 3.14159265359;

void main() {
    // Convert TexCoords from [0,1] to [-1,1] (NDC)
    vec2 ndc = TexCoords * 2.0 - 1.0;

    // For a 90° FOV, compute the ray direction in camera space.
    float fov = radians(90.0);
    float tanHalfFov = tan(fov * 0.5);
    // Assume the camera looks along -Z in camera space.
    vec3 rayDir = normalize(vec3(ndc.x * tanHalfFov, ndc.y * tanHalfFov, -1.0));

    // Use the inverse rotation from the view matrix.
    mat3 invRotation = transpose(mat3(viewMatrix));
    rayDir = normalize(invRotation * rayDir);

    // Apply an extra correction rotation about the Y-axis by -90°
    // Correction matrix: rotates the ray by -90° around Y.
    mat3 correction = mat3(
         0.0, 0.0, -1.0,
         0.0, 1.0,  0.0,
         1.0, 0.0,  0.0
    );
    rayDir = normalize(correction * rayDir);

    // Flip forward/backward: invert the Z component.
    rayDir.z = -rayDir.z;

    // Convert the corrected world-space ray to spherical coordinates.
    float lon = atan(rayDir.x, rayDir.z); // Longitude in [-PI, PI]
    float lat = asin(rayDir.y);           // Latitude in [-PI/2, PI/2]

    // Map spherical coordinates to equirectangular texture coordinates.
    float u = (lon / (2.0 * PI)) + 0.5;
    float v = (lat / PI) + 0.5;

    vec3 color = texture(uEquirect, vec2(u, v)).rgb;
    FragColor = vec4(color, 1.0);
}