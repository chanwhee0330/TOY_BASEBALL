#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 fragmentColor;
in vec3 emission;

uniform vec3 lightPos;
uniform vec3 lightColor;

out vec4 color;

void main()
{
    vec3 ambient = 0.42 * lightColor;
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);
    float diffuseAmount = max(dot(N, L), 0.0);
    vec3 diffuse = 0.58 * diffuseAmount * lightColor;
    vec3 result = (ambient + diffuse) * fragmentColor + emission;
    color = vec4(result, 1.0);
}
