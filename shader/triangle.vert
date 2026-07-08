#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 0) out vec2 texCoord;

layout(push_constant) uniform Push {
    mat4 model;
} push;

void main() {
    gl_Position = push.model * vec4(aPos, 1.0);
    texCoord = aTexCoord;
}
