#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aJointNum;
layout (location = 4) in vec4 aJointWeight;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;

layout(push_constant) uniform Push {
    mat4 mvp;
    int aModelStride;
} push;

layout (std430, set = 1, binding = 0) readonly buffer JointMatrices {
    mat4 jointMat[];
};

void main() {
    mat4 skinMat =
        aJointWeight.x * jointMat[int(aJointNum.x) + gl_InstanceIndex * push.aModelStride] +
        aJointWeight.y * jointMat[int(aJointNum.y) + gl_InstanceIndex * push.aModelStride] +
        aJointWeight.z * jointMat[int(aJointNum.z) + gl_InstanceIndex * push.aModelStride] +
        aJointWeight.w * jointMat[int(aJointNum.w) + gl_InstanceIndex * push.aModelStride];

    gl_Position = push.mvp * skinMat * vec4(aPos, 1.0);
    normal = normalize(vec3(skinMat * vec4(aNormal, 0.0)));
    texCoord = aTexCoord;
}
