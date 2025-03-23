#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

// Instance attributes: model matrix (locations 2,3,4,5)
layout(location = 2) in vec4 instanceMatRow0;
layout(location = 3) in vec4 instanceMatRow1;
layout(location = 4) in vec4 instanceMatRow2;
layout(location = 5) in vec4 instanceMatRow3;
// Instance attribute: color (location 6)
layout(location = 6) in vec3 instanceColor;

uniform mat4 uMVP;              // Combined view-projection matrix
uniform mat4 uLightSpaceMatrix; // For shadow mapping

out vec4 FragPosLightSpace;
out vec3 FragNormal;
out vec3 InstanceColor; // Pass instance color to fragment shader

void main()
{
    // Reconstruct the model matrix from instance attributes
    mat4 instanceModel = mat4(instanceMatRow0, instanceMatRow1, instanceMatRow2, instanceMatRow3);
    gl_Position = uMVP * instanceModel * vec4(aPos, 1.0);
    FragPosLightSpace = uLightSpaceMatrix * instanceModel * vec4(aPos, 1.0);
    FragNormal = mat3(transpose(inverse(instanceModel))) * aNormal;
    InstanceColor = instanceColor;
}