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

private:
    std::vector<std::shared_ptr<GltfNode>> mNodes{};
    unsigned int mIterations = 15;
    float mThreshold = 0.0001f;
};
