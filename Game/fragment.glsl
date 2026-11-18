#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 fragmentColor;
in vec3 emission;

uniform vec3 lightPositions[4];
uniform bool receiveBallShadow;
uniform vec3 ballPosition;
uniform float ballRadius;
uniform bool depthPass;
uniform vec3 sparkColor;
uniform bool useShadowMap;
uniform mat4 lightViewProjection;
uniform sampler2DShadow shadowMap;

out vec4 color;

float ballVisibility(vec3 lightDirection, float lightDistance)
{
    if (!receiveBallShadow) return 1.0;

    // 표면에서 조명으로 향하는 선분과 공 중심 사이의 최단 거리.
    vec3 toBall = ballPosition - FragPos;
    // 넓은 조명에서는 공이 표면에서 멀어질수록 가려지는 빛의 비중이 줄어든다.
    float opacity = 1.0 - smoothstep(0.20, 1.05, toBall.y);
    if (opacity <= 0.0) return 1.0;

    float travel = dot(toBall, lightDirection);
    if (travel <= 0.0 || travel >= lightDistance) return 1.0;

    float distanceToRay = length(toBall - lightDirection * travel);
    float penumbra = min(0.08, 0.015 + 0.025 * travel);
    float visibility = smoothstep(ballRadius - penumbra, ballRadius + penumbra, distanceToRay);
    return mix(1.0, visibility, opacity);
}

float stadiumVisibility()
{
    if (!useShadowMap) return 1.0;

    vec4 lightClip = lightViewProjection * vec4(FragPos, 1.0);
    if (lightClip.w <= 0.0) return 1.0;
    vec3 projected = lightClip.xyz / lightClip.w * 0.5 + 0.5;
    if (any(lessThanEqual(projected, vec3(0.0))) ||
        any(greaterThanEqual(projected, vec3(1.0)))) return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 sampleUV = projected.xy + vec2(x, y) * texel;
            visibility += texture(shadowMap, vec3(sampleUV, projected.z - 0.00015));
        }
    }
    return visibility / 9.0;
}

void main()
{
    if (depthPass)
    {
        color = vec4(0.0);
        return;
    }
    if (any(greaterThan(sparkColor, vec3(0.0))))
    {
        color = vec4(sparkColor, 1.0);
        return;
    }

    const float weights[4] = float[4](0.37, 0.17, 0.13, 0.13);
    const vec3 lightColor = vec3(1.0, 0.98, 0.94);
    vec3 N = normalize(Normal);
    vec3 lighting = vec3(0.30);
    for (int i = 0; i < 4; ++i)
    {
        vec3 toLight = lightPositions[i] - FragPos;
        float distanceToLight = length(toLight);
        vec3 L = toLight / distanceToLight;
        float diffuse = max(dot(N, L), 0.0);
        float visibility = ballVisibility(L, distanceToLight);
        if (i == 0) visibility *= stadiumVisibility();
        lighting += weights[i] * lightColor * diffuse * visibility;
    }
    vec3 result = lighting * fragmentColor + emission;
    color = vec4(result, 1.0);
}
