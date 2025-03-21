#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uEquirect;  // The equirectangular texture
uniform mat4 viewMatrix;      // The FPS camera's view matrix (world-to-camera)
uniform float uFactor;        // 0 = 90° perspective, 1 = full spherical (180°)

const float PI = 3.14159265359;

void main() {
    // --- 1. Compute the 90° perspective ray (camera space) ---
    vec2 ndc = TexCoords * 2.0 - 1.0; // NDC from [-1,1]
    float fov = radians(90.0);
    float tanHalfFov = tan(fov * 0.5);
    vec3 rayDirPersp = normalize(vec3(ndc.x * tanHalfFov, ndc.y * tanHalfFov, -1.0));

    // --- 2. Compute the full spherical ray (camera space) ---
    // For the full spherical branch we want:
    //   - Horizontal: full 360° mapping, with center (TexCoords.x == 0.5) mapping to forward.
    //   - Vertical: mapping TexCoords.y in [0,1] to [-pi/2, pi/2] so that 0.5 is zero.
    // Thus:
    float phi_full = (TexCoords.x - 0.5) * 2.0 * PI - (PI / 2.0); // center at -pi/2 yields forward.
    float theta_full = (TexCoords.y - 0.5) * PI;                    // range [-pi/2, pi/2]
    vec3 rayDirFull = normalize(vec3(
        cos(theta_full) * cos(phi_full),
        sin(theta_full),
        cos(theta_full) * sin(phi_full)
    ));

    // --- 3. Interpolate between the two rays ---
    // uFactor = 0 gives the original 90° perspective ray;
    // uFactor = 1 gives the full spherical ray.
    vec3 rayDir = normalize(mix(rayDirPersp, rayDirFull, uFactor));

    // --- 4. Transform the ray from camera space to world space ---
    mat3 invRotation = transpose(mat3(viewMatrix));
    rayDir = normalize(invRotation * rayDir);

    // --- 5. Apply a correction rotation about the Y-axis by -90° ---
    mat3 correction = mat3(
         0.0, 0.0, -1.0,
         0.0, 1.0,  0.0,
         1.0, 0.0,  0.0
    );
    rayDir = normalize(correction * rayDir);

    // Flip forward/backward: invert the Z component.
    rayDir.z = -rayDir.z;

    // --- 6. Convert the final ray direction to spherical coordinates ---
    float lon = atan(rayDir.x, rayDir.z); // Longitude in [-PI, PI]
    float lat = asin(rayDir.y);           // Latitude in [-PI/2, PI/2]

    // Map spherical coordinates to equirectangular texture coordinates.
    float u = (lon / (2.0 * PI)) + 0.5;
    float v = (lat / PI) + 0.5;

    vec3 color = texture(uEquirect, vec2(u, v)).rgb;
    FragColor = vec4(color, 1.0);
}