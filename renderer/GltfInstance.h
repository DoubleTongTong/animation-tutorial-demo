#pragma once
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "GltfNode.h"
#include "GltfAnimationClip.h"
#include "IKSolver.h"
#include "ModelSettings.h"

class GltfModel;

class GltfInstance {
public:
    GltfInstance(std::shared_ptr<GltfModel> model, glm::vec2 worldPos, bool randomize = false);

    void resetNodeData();
    void setSkeletonSplitNode(int nodeNum);
    int getJointMatrixSize() const { return static_cast<int>(mInverseBindMatrices.size()); }

    const std::vector<glm::mat4>& getJointMatrices() const { return mJointMatrices; }
    const std::vector<glm::mat2x4>& getJointDualQuats() const { return mJointDualQuats; }

    std::shared_ptr<GltfModel> getModel() const { return mGltfModel; }
    void setModelInstanceIndex(int idx) { mModelInstanceIndex = idx; }
    int getModelInstanceIndex() const { return mModelInstanceIndex; }

    void updateAnimation();
    void solveIK();

    void setInstanceSettings(const ModelSettings& settings);
    ModelSettings getInstanceSettings() const { return mModelSettings; }
    void checkForUpdates();

    glm::vec2 getWorldPosition() const { return mModelSettings.msWorldPosition; }
    glm::quat getWorldRotation() const;

private:
    void playAnimation(int index, float speed, float blendFactor = 1.0f);
    void playAnimation(int sourceAnimNum, int destAnimNum, float speedDivider, float blendFactor);
    void blendAnimationFrame(int index, float timePos, float blendFactor);
    void crossBlendAnimationFrame(int sourceAnimNumber, int destAnimNumber, float time, float blendFactor);
    float getAnimationEndTime(int index);

    void updateNodeMatrices(std::shared_ptr<GltfNode> node);
    void updateJoints(std::shared_ptr<GltfNode> node, const glm::mat4& parentMatrix, float time);

    std::shared_ptr<GltfNode> mRootNode = nullptr;
    std::vector<std::shared_ptr<GltfNode>> mNodeList{};
    std::vector<glm::mat4> mJointMatrices{};
    std::vector<glm::mat2x4> mJointDualQuats{};
    std::vector<bool> mAdditiveAnimationMask{};
    std::vector<bool> mInvertedAdditiveAnimationMask{};

    std::shared_ptr<GltfModel> mGltfModel = nullptr;
    std::vector<GltfAnimationClip> mAnimClips{};
    std::vector<glm::mat4> mInverseBindMatrices{};
    std::vector<int> mNodeToJoint{};
    unsigned int mNodeCount = 0;

    ModelSettings mModelSettings{};
    IKSolver mIKSolver{};

    int mLastSplitNode = -1;
    int mLastEffector = -1;
    int mLastRoot = -1;
    int mModelInstanceIndex = 0;
};
