#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;

layout(push_constant) uniform Push {
    mat4 mvp;
} push;

void main() {
    gl_Position = push.mvp * vec4(aPos, 1.0);
    normal = aNormal;
    texCoord = aTexCoord;
}
