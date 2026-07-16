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

layout (std430, set = 1, binding = 0) readonly buffer JointDualQuats {
    mat2x4 jointDQs[];
};

// 四元数乘法
vec4 quat_mul(vec4 q1, vec4 q2) {
    return vec4(
        q1.w * q2.xyz + q2.w * q1.xyz + cross(q1.xyz, q2.xyz),
        q1.w * q2.w - dot(q1.xyz, q2.xyz)
    );
}

// 使用 DLB（双四元数线性混合）获取混合后的关节变换
mat2x4 getJointTransform(ivec4 joints, vec4 weights) {
    mat2x4 dq0 = jointDQs[joints.x + gl_InstanceIndex * push.aModelStride];
    mat2x4 dq1 = jointDQs[joints.y + gl_InstanceIndex * push.aModelStride];
    mat2x4 dq2 = jointDQs[joints.z + gl_InstanceIndex * push.aModelStride];
    mat2x4 dq3 = jointDQs[joints.w + gl_InstanceIndex * push.aModelStride];

    // 邻域对齐以解决对跖点歧义（确保沿最短路径插值）
    weights.y *= (dot(dq0[0], dq1[0]) < 0.0) ? -1.0 : 1.0;
    weights.z *= (dot(dq0[0], dq2[0]) < 0.0) ? -1.0 : 1.0;
    weights.w *= (dot(dq0[0], dq3[0]) < 0.0) ? -1.0 : 1.0;

    // 加权线性组合
    mat2x4 result = weights.x * dq0 +
                    weights.y * dq1 +
                    weights.z * dq2 +
                    weights.w * dq3;

    // 归一化以维持双四元数的单位化（刚性变换）
    float norm = length(result[0]);
    return result / norm;
}

// 从混合后的双四元数重建 4x4 蒙皮矩阵
mat4 getSkinMat() {
    mat2x4 bone = getJointTransform(ivec4(round(aJointNum)), aJointWeight);
    vec4 r = bone[0]; // 实部（旋转四元数）
    vec4 t = bone[1]; // 虚部（位移对偶部分）

    // 提取平移向量: trans = 2.0 * dual * conj(real)
    vec4 conj_r = vec4(-r.xyz, r.w);
    vec4 trans_quat = quat_mul(t, conj_r) * 2.0;
    vec3 translation = trans_quat.xyz;

    float x = r.x, y = r.y, z = r.z, w = r.w;

    return mat4(
        vec4(1.0 - 2.0 * (y*y + z*z),       2.0 * (x*y + w*z), 2.0 * (x*z - w*y), 0.0),
        vec4(2.0 * (x*y - w*z), 1.0 - 2.0 * (x*x + z*z),       2.0 * (y*z + w*x), 0.0),
        vec4(2.0 * (x*z + w*y),       2.0 * (y*z - w*x), 1.0 - 2.0 * (x*x + y*y), 0.0),
        vec4(translation, 1.0)
    );
}

void main() {
    mat4 skinMat = getSkinMat();

    gl_Position = push.mvp * skinMat * vec4(aPos, 1.0);
    normal = normalize(vec3(skinMat * vec4(aNormal, 0.0)));
    texCoord = aTexCoord;
}
