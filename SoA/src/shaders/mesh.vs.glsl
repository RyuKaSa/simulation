#version 330 core

// Input vertex attributes.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

// Uniforms for transformation.
uniform mat4 uModel;
uniform mat4 uMVP;
uniform mat4 uLightSpaceMatrix;

// Outputs to the fragment shader.
out vec4 FragPosLightSpace;
out vec3 FragNormal;
out vec3 InstanceColor;

void main()
{
    // Compute the world-space position.
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    
    // Use the transformed world position for gl_Position.
    gl_Position = uMVP * worldPos;
    
    // Compute the fragment position in light space for shadow mapping.
    FragPosLightSpace = uLightSpaceMatrix * worldPos;
    
    // Transform the normal using the transpose of the inverse of the model matrix.
    FragNormal = mat3(transpose(inverse(uModel))) * aNormal;
    
    // Pass through the vertex (or instance) color.
    InstanceColor = aColor;
}