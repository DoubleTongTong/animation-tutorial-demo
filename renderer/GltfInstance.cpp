#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <GLFW/glfw3.h>
#include <cstdlib>
#include "GltfInstance.h"
#include "GltfModel.h"
#include "Logger.h"

GltfInstance::GltfInstance(std::shared_ptr<GltfModel> model, glm::vec2 worldPos, bool randomize) {
    if (!model) {
        Logger::log(1, "%s error: invalid glTF model\n", __FUNCTION__);
        return;
    }

    mGltfModel = model;
    mModelSettings.msWorldPosition = worldPos;

    mNodeCount = mGltfModel->getNodeCount();
    mInverseBindMatrices = mGltfModel->getInverseBindMatrices();
    mNodeToJoint = mGltfModel->getNodeToJoint();

    mJointMatrices.resize(mInverseBindMatrices.size(), glm::mat4(1.0f));
    mJointDualQuats.resize(mInverseBindMatrices.size());
    mAdditiveAnimationMask.resize(mNodeCount, true);
    mInvertedAdditiveAnimationMask.resize(mNodeCount, false);

    std::fill(mAdditiveAnimationMask.begin(), mAdditiveAnimationMask.end(), true);
    mInvertedAdditiveAnimationMask = mAdditiveAnimationMask;
    mInvertedAdditiveAnimationMask.flip();

    // 载入节点树
    mRootNode = mGltfModel->buildInstanceNodeTree(mNodeList);
    mRootNode->setTranslation(glm::vec3(mModelSettings.msWorldPosition.x, 0.0f, mModelSettings.msWorldPosition.y));
    mRootNode->setRotation(getWorldRotation());
    updateNodeMatrices(mRootNode);

    mModelSettings.msSkelSplitNode = mNodeCount - 1;
    for (const auto &node : mNodeList) {
        if (node) {
            mModelSettings.msSkelSplitNodeNames.push_back(node->getNodeName());
        } else {
            mModelSettings.msSkelSplitNodeNames.push_back("(invalid)");
        }
    }

    mAnimClips = mGltfModel->getAnimClips();
    for (const auto &clip : mAnimClips) {
        mModelSettings.msClipNames.push_back(clip.getClipName());
    }

    if (randomize && !mAnimClips.empty()) {
        int animClip = std::rand() % mAnimClips.size();
        float animClipSpeed = (std::rand() % 100) / 100.0f + 0.5f;
        float initRotation = static_cast<float>(std::rand() % 360 - 180);
        mModelSettings.msAnimClip = animClip;
        mModelSettings.msAnimSpeed = animClipSpeed;
        mModelSettings.msWorldRotation = glm::vec3(0.0f, initRotation, 0.0f);
    }

    // 默认IK设置 (以右臂/手为例)
    mModelSettings.msIkEffectorNode = 19;
    mModelSettings.msIkRootNode = 26;
    if (mModelSettings.msIkEffectorNode >= static_cast<int>(mNodeList.size())) {
        mModelSettings.msIkEffectorNode = 0;
    }
    if (mModelSettings.msIkRootNode >= static_cast<int>(mNodeList.size())) {
        mModelSettings.msIkRootNode = 0;
    }

    mModelSettings.msIkTargetWorldPos = getWorldRotation() * mModelSettings.msIkTargetPos +
                                        glm::vec3(worldPos.x, 0.0f, worldPos.y);

    checkForUpdates();
}

void GltfInstance::resetNodeData() {
    if (mRootNode && mGltfModel) {
        mGltfModel->getNodeData(mRootNode, glm::mat4(1.0f));

        auto resetNodes = [&](auto& self, std::shared_ptr<GltfNode> treeNode) -> void {
            if (!treeNode) return;
            for (auto &childNode : treeNode->mChildNodes) {
                mGltfModel->getNodeData(childNode, treeNode->mNodeMatrix);
                self(self, childNode);
            }
        };
        resetNodes(resetNodes, mRootNode);
    }
}

void GltfInstance::setSkeletonSplitNode(int nodeNum) {
    std::fill(mAdditiveAnimationMask.begin(), mAdditiveAnimationMask.end(), true);

    auto updateAdditiveMask = [&](auto& self, std::shared_ptr<GltfNode> treeNode, int splitNodeNum) -> void {
        if (!treeNode) return;
        if (treeNode->getNodeNum() == splitNodeNum) {
            return;
        }
        mAdditiveAnimationMask.at(treeNode->getNodeNum()) = false;
        for (auto &childNode : treeNode->mChildNodes) {
            self(self, childNode, splitNodeNum);
        }
    };
    updateAdditiveMask(updateAdditiveMask, mRootNode, nodeNum);
    mInvertedAdditiveAnimationMask = mAdditiveAnimationMask;
    mInvertedAdditiveAnimationMask.flip();
}

void GltfInstance::updateAnimation() {
    float time = static_cast<float>(glfwGetTime());

    bool useCrossBlending = (mModelSettings.msBlendingMode == blendMode::crossfade || mModelSettings.msBlendingMode == blendMode::additive);

    if (mModelSettings.msPlayAnimation) {
        if (useCrossBlending) {
            playAnimation(mModelSettings.msAnimClip, mModelSettings.msCrossBlendDestAnimClip, mModelSettings.msAnimSpeed, mModelSettings.msAnimCrossBlendFactor);
        } else {
            playAnimation(mModelSettings.msAnimClip, mModelSettings.msAnimSpeed, mModelSettings.msAnimBlendFactor);
        }
    } else {
        mModelSettings.msAnimEndTime = getAnimationEndTime(mModelSettings.msAnimClip);
        if (useCrossBlending) {
            crossBlendAnimationFrame(mModelSettings.msAnimClip, mModelSettings.msCrossBlendDestAnimClip, mModelSettings.msAnimTimePosition, mModelSettings.msAnimCrossBlendFactor);
        } else {
            blendAnimationFrame(mModelSettings.msAnimClip, mModelSettings.msAnimTimePosition, mModelSettings.msAnimBlendFactor);
        }
    }

    solveIK();

    updateJoints(mRootNode, glm::mat4(1.0f), time);

    // 计算双四元数
    mJointDualQuats.resize(mJointMatrices.size());
    for (size_t i = 0; i < mJointMatrices.size(); ++i) {
        glm::quat orientation;
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::dualquat dq;
        if (glm::decompose(mJointMatrices[i], scale, orientation, translation, skew, perspective)) {
            orientation = glm::normalize(orientation);
            if (orientation.w < 0.0f) {
                orientation = -orientation;
            }
            dq[0] = orientation;
            dq[1] = glm::quat(0.0f, translation.x, translation.y, translation.z) * orientation * 0.5f;
            mJointDualQuats[i] = glm::mat2x4_cast(dq);
        } else {
            dq[0] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            dq[1] = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
            mJointDualQuats[i] = glm::mat2x4_cast(dq);
        }
    }
}

void GltfInstance::solveIK() {
    if (mModelSettings.msIkMode == ikMode::off) {
        return;
    }
    updateNodeMatrices(mRootNode);
    mIKSolver.setNumIterations(mModelSettings.msIkIterations);

    if (mModelSettings.msIkMode == ikMode::ccd) {
        mIKSolver.solveCCD(mModelSettings.msIkTargetPos);
    } else if (mModelSettings.msIkMode == ikMode::fabrik) {
        mIKSolver.solveFABRIK(mModelSettings.msIkTargetPos);
    }
    updateNodeMatrices(mIKSolver.getIkChainRootNode());
}

void GltfInstance::setInstanceSettings(const ModelSettings& settings) {
    mModelSettings = settings;
    mModelSettings.msIkTargetWorldPos = getWorldRotation() * mModelSettings.msIkTargetPos +
                                        glm::vec3(mModelSettings.msWorldPosition.x, 0.0f, mModelSettings.msWorldPosition.y);
}

void GltfInstance::checkForUpdates() {
    static thread_local int s_lastSplitNode = -1; // 用简单的单实例标记或者成员变量更新
    // 我们在GltfInstance中直接使用成员变量进行差异对比：
    if (mLastSplitNode != mModelSettings.msSkelSplitNode) {
        setSkeletonSplitNode(mModelSettings.msSkelSplitNode);
        resetNodeData();
        mLastSplitNode = mModelSettings.msSkelSplitNode;
    }

    if (mLastEffector != mModelSettings.msIkEffectorNode || mLastRoot != mModelSettings.msIkRootNode) {
        std::vector<std::shared_ptr<GltfNode>> ikNodes{};
        if (mModelSettings.msIkEffectorNode >= 0 && mModelSettings.msIkEffectorNode < static_cast<int>(mNodeList.size()) &&
            mModelSettings.msIkRootNode >= 0 && mModelSettings.msIkRootNode < static_cast<int>(mNodeList.size())) {
            ikNodes.push_back(mNodeList.at(mModelSettings.msIkEffectorNode));
            int currentNodeNum = mModelSettings.msIkEffectorNode;
            while (currentNodeNum != mModelSettings.msIkRootNode) {
                std::shared_ptr<GltfNode> node = mNodeList.at(currentNodeNum);
                if (node) {
                    std::shared_ptr<GltfNode> parentNode = node->getParentNode();
                    if (parentNode) {
                        currentNodeNum = parentNode->getNodeNum();
                        ikNodes.push_back(parentNode);
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        mIKSolver.setNodes(ikNodes);
        resetNodeData();
        mLastEffector = mModelSettings.msIkEffectorNode;
        mLastRoot = mModelSettings.msIkRootNode;
    }

    // 同步根节点位置与旋转
    if (mRootNode) {
        mRootNode->setTranslation(glm::vec3(mModelSettings.msWorldPosition.x, 0.0f, mModelSettings.msWorldPosition.y));
        mRootNode->setRotation(getWorldRotation());
    }
}

glm::quat GltfInstance::getWorldRotation() const {
    // 假设msWorldRotation包含度数单位下的Y轴旋转 (msWorldRotation.y)
    return glm::angleAxis(glm::radians(mModelSettings.msWorldRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
}

void GltfInstance::playAnimation(int index, float speed, float blendFactor) {
    if (index >= 0 && index < static_cast<int>(mAnimClips.size())) {
        float time = static_cast<float>(glfwGetTime());
        float clipTime = std::fmod(time * speed, mAnimClips[index].getClipEndTime());
        blendAnimationFrame(index, clipTime, blendFactor);
        mModelSettings.msAnimTimePosition = clipTime;
    }
}

void GltfInstance::playAnimation(int sourceAnimNum, int destAnimNum, float speedDivider, float blendFactor) {
    if (sourceAnimNum >= 0 && sourceAnimNum < static_cast<int>(mAnimClips.size()) &&
        destAnimNum >= 0 && destAnimNum < static_cast<int>(mAnimClips.size())) {
        float time = static_cast<float>(glfwGetTime());
        float sourceEndTime = mAnimClips[sourceAnimNum].getClipEndTime();
        float clipTime = std::fmod(time * speedDivider, sourceEndTime);

        crossBlendAnimationFrame(sourceAnimNum, destAnimNum, clipTime, blendFactor);
        mModelSettings.msAnimTimePosition = clipTime;
    }
}

void GltfInstance::blendAnimationFrame(int index, float timePos, float blendFactor) {
    if (index >= 0 && index < static_cast<int>(mAnimClips.size())) {
        mAnimClips.at(index).blendAnimationFrame(mNodeList, mAdditiveAnimationMask, timePos, blendFactor);
    }
}

void GltfInstance::crossBlendAnimationFrame(int sourceAnimNumber, int destAnimNumber, float time, float blendFactor) {
    if (sourceAnimNumber >= 0 && sourceAnimNumber < static_cast<int>(mAnimClips.size()) &&
        destAnimNumber >= 0 && destAnimNumber < static_cast<int>(mAnimClips.size())) {
        float sourceAnimDuration = mAnimClips[sourceAnimNumber].getClipEndTime();
        float destAnimDuration = mAnimClips[destAnimNumber].getClipEndTime();

        float scaledTime = time * (destAnimDuration / sourceAnimDuration);

        mAnimClips.at(sourceAnimNumber).setAnimationFrame(mNodeList, mAdditiveAnimationMask, time);
        mAnimClips.at(destAnimNumber).blendAnimationFrame(mNodeList, mAdditiveAnimationMask, scaledTime, blendFactor);

        mAnimClips.at(destAnimNumber).setAnimationFrame(mNodeList, mInvertedAdditiveAnimationMask, scaledTime);
        mAnimClips.at(sourceAnimNumber).blendAnimationFrame(mNodeList, mInvertedAdditiveAnimationMask, time, blendFactor);
    }
}

float GltfInstance::getAnimationEndTime(int index) {
    if (index >= 0 && index < static_cast<int>(mAnimClips.size())) {
        return mAnimClips[index].getClipEndTime();
    }
    return 0.0f;
}

void GltfInstance::updateNodeMatrices(std::shared_ptr<GltfNode> node) {
    if (!node) return;

    node->calculateNodeMatrix();

    if (node->mNodeIndex >= 0 && node->mNodeIndex < static_cast<int>(mNodeToJoint.size())) {
        int jointIdx = mNodeToJoint[node->mNodeIndex];
        if (jointIdx != -1) {
            mJointMatrices[jointIdx] = node->mNodeMatrix * mInverseBindMatrices[jointIdx];
        }
    }

    for (const auto& child : node->mChildNodes) {
        updateNodeMatrices(child);
    }
}

void GltfInstance::updateJoints(std::shared_ptr<GltfNode> node, const glm::mat4& parentMatrix, float time) {
    if (!node) return;

    node->calculateLocalTRSMatrix();
    node->mNodeMatrix = parentMatrix * node->mLocalTRSMatrix;

    if (node->mNodeIndex >= 0 && node->mNodeIndex < static_cast<int>(mNodeToJoint.size())) {
        int jointIdx = mNodeToJoint[node->mNodeIndex];
        if (jointIdx != -1) {
            mJointMatrices[jointIdx] = node->mNodeMatrix * mInverseBindMatrices[jointIdx];
        }
    }

    for (const auto& child : node->mChildNodes) {
        updateJoints(child, node->mNodeMatrix, time);
    }
}
