#version 330 core

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vEmission;

uniform mat4 modelTransform;
uniform mat4 model;

out vec3 FragPos;
out vec3 Normal;
out vec3 fragmentColor;
out vec3 emission;

void main()
{
    gl_Position = modelTransform * vec4(vPos, 1.0);

    FragPos = vec3(model * vec4(vPos, 1.0));
    Normal = transpose(inverse(mat3(model))) * vNormal;
    fragmentColor = vColor;
    emission = vEmission;
}
