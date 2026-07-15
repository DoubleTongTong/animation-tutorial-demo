#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "GltfNode.h"

class IKSolver {
public:
    IKSolver() = default;
    IKSolver(unsigned int iterations) : mIterations(iterations) {}

    void setNodes(std::vector<std::shared_ptr<GltfNode>> nodes) {
        mNodes = nodes;
        calculateBoneLengths();
        mFABRIKNodePositions.resize(mNodes.size());
    }

    std::shared_ptr<GltfNode> getIkChainRootNode() {
        if (mNodes.empty()) {
            return nullptr;
        }
        return mNodes.back();
    }

    void setNumIterations(unsigned int iterations) {
        mIterations = iterations;
    }

    bool solveCCD(glm::vec3 target);
    bool solveFABRIK(glm::vec3 target);

private:
    void solveFABRIKForward(glm::vec3 target);
    void solveFABRIKBackward(glm::vec3 base);
    void calculateBoneLengths();
    void adjustFABRIKNodes();

    std::vector<std::shared_ptr<GltfNode>> mNodes{};
    unsigned int mIterations = 15;
    float mThreshold = 0.0001f;

    std::vector<float> mBoneLengths{};
    std::vector<glm::vec3> mFABRIKNodePositions{};
};
