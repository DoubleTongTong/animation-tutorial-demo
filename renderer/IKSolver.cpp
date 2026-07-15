#define GLM_ENABLE_EXPERIMENTAL
#include "IKSolver.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

bool IKSolver::solveCCD(const glm::vec3 target) {
    if (mNodes.empty()) {
        return false;
    }

    for (unsigned int i = 0; i < mIterations; ++i) {
        glm::vec3 effector = mNodes.at(0)->getGlobalPosition();
        if (glm::length(target - effector) < mThreshold) {
            return true;
        }

        for (size_t j = 1; j < mNodes.size(); ++j) {
            std::shared_ptr<GltfNode> node = mNodes.at(j);
            if (!node) {
                continue;
            }

            glm::vec3 position = node->getGlobalPosition();
            glm::quat rotation = node->getGlobalRotation();

            glm::vec3 toEffector = glm::normalize(effector - position);
            glm::vec3 toTarget = glm::normalize(target - position);

            glm::quat effectorToTarget = glm::rotation(toEffector, toTarget);

            // Translate global rotation difference to local space
            glm::quat localRotation = rotation * effectorToTarget * glm::conjugate(rotation);

            glm::quat currentRotation = node->getLocalRotation();
            node->blendRotation(currentRotation * localRotation, 1.0f);

            // Update hierarchy starting from current node down to the children
            node->updateNodeAndChildMatrices();

            // Re-evaluate effector position after adjusting this node
            effector = mNodes.at(0)->getGlobalPosition();
            if (glm::length(target - effector) < mThreshold) {
                return true;
            }
        }
    }
    return false;
}

void IKSolver::calculateBoneLengths() {
    if (mNodes.size() < 2) return;
    mBoneLengths.resize(mNodes.size() - 1);
    for (size_t i = 0; i < mNodes.size() - 1; ++i) {
        std::shared_ptr<GltfNode> startNode = mNodes.at(i);
        std::shared_ptr<GltfNode> endNode = mNodes.at(i + 1);
        mBoneLengths.at(i) = glm::length(endNode->getGlobalPosition() - startNode->getGlobalPosition());
    }
}

void IKSolver::solveFABRIKForward(glm::vec3 target) {
    mFABRIKNodePositions.at(0) = target;
    for (size_t i = 1; i < mFABRIKNodePositions.size(); ++i) {
        glm::vec3 boneDirection = glm::normalize(
            mFABRIKNodePositions.at(i) - mFABRIKNodePositions.at(i - 1));
        glm::vec3 offset = boneDirection * mBoneLengths.at(i - 1);
        mFABRIKNodePositions.at(i) = mFABRIKNodePositions.at(i - 1) + offset;
    }
}

void IKSolver::solveFABRIKBackward(glm::vec3 base) {
    mFABRIKNodePositions.at(mFABRIKNodePositions.size() - 1) = base;
    for (int i = static_cast<int>(mFABRIKNodePositions.size()) - 2; i >= 0; --i) {
        glm::vec3 boneDirection = glm::normalize(
            mFABRIKNodePositions.at(i) - mFABRIKNodePositions.at(i + 1));
        glm::vec3 offset = boneDirection * mBoneLengths.at(i);
        mFABRIKNodePositions.at(i) = mFABRIKNodePositions.at(i + 1) + offset;
    }
}

void IKSolver::adjustFABRIKNodes() {
    for (size_t i = mFABRIKNodePositions.size() - 1; i > 0; --i) {
        std::shared_ptr<GltfNode> node = mNodes.at(i);
        std::shared_ptr<GltfNode> nextNode = mNodes.at(i - 1);

        glm::vec3 position = node->getGlobalPosition();
        glm::quat rotation = node->getGlobalRotation();
        glm::vec3 nextPosition = nextNode->getGlobalPosition();

        glm::vec3 toNext = glm::normalize(nextPosition - position);
        glm::vec3 toDesired = glm::normalize(
            mFABRIKNodePositions.at(i - 1) - mFABRIKNodePositions.at(i));

        glm::quat nodeRotation = glm::rotation(toNext, toDesired);
        glm::quat localRotation = rotation * nodeRotation * glm::conjugate(rotation);

        glm::quat currentRotation = node->getLocalRotation();
        node->blendRotation(currentRotation * localRotation, 1.0f);

        node->updateNodeAndChildMatrices();
    }
}

bool IKSolver::solveFABRIK(glm::vec3 target) {
    if (mNodes.empty()) {
        return false;
    }

    mFABRIKNodePositions.resize(mNodes.size());
    for (size_t i = 0; i < mNodes.size(); ++i) {
        mFABRIKNodePositions.at(i) = mNodes.at(i)->getGlobalPosition();
    }

    glm::vec3 base = getIkChainRootNode()->getGlobalPosition();

    for (unsigned int i = 0; i < mIterations; ++i) {
        glm::vec3 effector = mFABRIKNodePositions.at(0);
        if (glm::length(target - effector) < mThreshold) {
            adjustFABRIKNodes();
            return true;
        }

        solveFABRIKForward(target);
        solveFABRIKBackward(base);
    }

    adjustFABRIKNodes();

    glm::vec3 effector = mNodes.at(0)->getGlobalPosition();
    return glm::length(target - effector) < mThreshold;
}
