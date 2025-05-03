#version 330 core

in vec4 FragPosLightSpace;  // position in light's clip space
in vec3 FragNormal;         // surface normal in world space

out vec4 FragColor;

uniform sampler2D uShadowMap;
uniform vec3 uLightDir;     

void main()
{
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; 

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        float ndotl = max(dot(normalize(FragNormal), uLightDir), 0.0);
        // Slightly larger ambient so it's clearly visible on a grey background
        float litShade = 0.3 + 0.7 * ndotl;
        FragColor = vec4(litShade, litShade, litShade, 1.0);
        return;
    }

    float currentDepth = projCoords.z;
    float closestDepth = texture(uShadowMap, projCoords.xy).r;

    // A small bias helps prevent shadow acne:
    float bias = 0.0005;
    float shadow = (currentDepth > closestDepth + bias) ? 1.0 : 0.0;

    float ndotl = dot(normalize(FragNormal), uLightDir);
    ndotl = clamp(ndotl, 0.0, 1.0);

    // Lit shade: 0.3 ambient + 0.7 * N·L
    float litShade = 0.3 + 0.7 * ndotl;

    // Shadow shade: still visible but darker
    float shadowShade = 0.15 + 0.3 * ndotl;

    // Mix between litShade and shadowShade
    float finalShade = mix(litShade, shadowShade, shadow);

    // Output as a simple grayscale
    FragColor = vec4(finalShade, finalShade, finalShade, 1.0);
}