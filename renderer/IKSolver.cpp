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
