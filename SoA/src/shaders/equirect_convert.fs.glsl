#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform samplerCube uCubemap;

const float PI = 3.14159265359;

void main() {
    // Convert TexCoords from [0,1] to [-1,1]
    vec2 uv = TexCoords * 2.0 - 1.0;
    
    // Compute the central spherical coordinates:
    float phi0   = uv.x * PI;         // horizontal: -pi to pi
    float theta0 = uv.y * (PI / 2.0);   // vertical: -pi/2 to pi/2

    // Kernel settings:
    const int kernelSize = 7;
    vec3 accum = vec3(0.0);
    float samples = 0.0;
    // Offsets in spherical space (radians); adjust these values to control the filter size.
    float offsetPhi = 0.0005;   // tweak as needed
    float offsetTheta = 0.0005; // tweak as needed

    for (int i = 0; i < kernelSize; i++) {
        for (int j = 0; j < kernelSize; j++) {
            float dPhi = (float(i) - float(kernelSize - 1) / 2.0) * offsetPhi;
            float dTheta = (float(j) - float(kernelSize - 1) / 2.0) * offsetTheta;
            float phi = phi0 + dPhi;
            float theta = theta0 + dTheta;
            
            // Convert the adjusted spherical coordinates back to Cartesian:
            float x = cos(theta) * cos(phi);
            float y = sin(theta);
            float z = cos(theta) * sin(phi);
            vec3 dir = normalize(vec3(x, y, z));
            
            accum += texture(uCubemap, dir).rgb;
            samples += 1.0;
        }
    }
    
    vec3 color = accum / samples;
    FragColor = vec4(color, 1.0);
}