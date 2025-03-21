#version 330 core
in vec4 FragPosLightSpace;
out vec4 FragColor;

uniform vec3 uColor;       // the color of this object
uniform sampler2D uShadowMap;
uniform vec3 uLightDir;

float computeShadow(vec4 lightSpacePos)
{
    // Perspective divide (or for directional orth, same procedure):
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    // Transform to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // If outside the light’s orthographic region, consider it unshadowed:
    if(projCoords.x < 0.0 || projCoords.x > 1.0 ||
       projCoords.y < 0.0 || projCoords.y > 1.0 )
    {
        return 0.0;
    }

    // Depth in the shadow map
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    // Current fragment’s depth
    float currentDepth = projCoords.z;
    // Add a small bias
    float bias = 0.005;
    // 1.0 if in shadow, else 0.0
    return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

void main()
{
    // a trivial diffuse factor could be e.g. dot(N, LightDir), omitted for brevity
    float shadow = computeShadow(FragPosLightSpace);
    // e.g. darken by 50% if in shadow
    vec3 colorOut = uColor * (1.0 - 0.5 * shadow);

    FragColor = vec4(colorOut, 1.0);
}