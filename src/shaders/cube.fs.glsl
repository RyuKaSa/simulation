#version 330 core

in vec4 FragPosLightSpace;
in vec3 FragNormal;
in vec3 InstanceColor;  // Added instance color input

out vec4 FragColor;

uniform sampler2D uShadowMap;
uniform vec3 uLightDir;   // Light direction (normalized) pointing FROM the surface TO the light.

void main()
{
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        float ndotl = max(dot(normalize(FragNormal), -uLightDir), 0.0);
        float litShade = 0.2 + 0.8 * ndotl;
        FragColor = vec4(InstanceColor * litShade, 1.0);
        return;
    }

    float currentDepth = projCoords.z;
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    
    float bias = 0.0005;
    
    // Shadow factor: 0 = not in shadow, 1 = fully shadowed
    float shadow = (currentDepth > closestDepth + bias) ? 1.0 : 0.0;

    float ndotl = dot(normalize(FragNormal), -uLightDir);
    ndotl = clamp(ndotl, 0.0, 1.0);

    float litShade = 0.2 + 0.8 * ndotl;

    float shadowShade = 0.1 + 0.3 * ndotl;

    float finalShade = mix(litShade, shadowShade, shadow);

    FragColor = vec4(InstanceColor * finalShade, 1.0);
}