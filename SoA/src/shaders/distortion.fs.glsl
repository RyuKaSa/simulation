#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uEquirect;  // The equirectangular texture
uniform mat4 viewMatrix;      // The FPS camera's view matrix (world-to-camera)
uniform float uFactor;        // 0 = 90° perspective, 1 = full spherical (180°)
uniform bool showCrosshair;   // Enable/disable crosshair display

const float PI = 3.14159265359;
const float aspectRatio = 16.0 / 9.0;

void main() {
    // --- 1. Compute the 90° perspective ray (camera space) ---
    vec2 ndc = TexCoords * 2.0 - 1.0; // NDC from [-1,1]
    ndc.x *= aspectRatio; // Correct aspect ratio
    float fov = radians(90.0);
    float tanHalfFov = tan(fov * 0.5);
    vec3 rayDirPersp = normalize(vec3(ndc.x * tanHalfFov, ndc.y * tanHalfFov, -1.0));

    // --- 2. Compute the full spherical ray (camera space) ---
    float phi_full = (TexCoords.x - 0.5) * 2.0 * PI - (PI / 2.0);
    float theta_full = (TexCoords.y - 0.5) * PI;
    vec3 rayDirFull = normalize(vec3(
        cos(theta_full) * cos(phi_full),
        sin(theta_full),
        cos(theta_full) * sin(phi_full)
    ));

    // --- 3. Interpolate between the two rays ---
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
    
    // --- 7. Overlay a center crosshair (+) if enabled ---
    if (showCrosshair) {
        // Estimate one pixel's size in texture space.
        vec2 pixelSize = fwidth(TexCoords);
        // For 2-pixel thick lines, half-thickness is 1 pixel.
        float halfThicknessX = pixelSize.x;
        float halfThicknessY = pixelSize.y;
        // Set arm half-length to 5 pixels (adjust multiplier for a different length).
        float armHalfLengthX = 5.0 * pixelSize.x;
        float armHalfLengthY = 5.0 * pixelSize.y;
        
        // Vertical line: very narrow in x and extending along y.
        bool inVertical = (abs(TexCoords.x - 0.5) < halfThicknessX) &&
                          (abs(TexCoords.y - 0.5) < armHalfLengthY);
        // Horizontal line: very narrow in y and extending along x.
        bool inHorizontal = (abs(TexCoords.y - 0.5) < halfThicknessY) &&
                            (abs(TexCoords.x - 0.5) < armHalfLengthX);
        
        // Invert the computed color if the fragment lies in either band.
        if (inVertical || inHorizontal) {
            color = vec3(1.0) - color;
        }
    }
    
    FragColor = vec4(color, 1.0);
}