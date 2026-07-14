#define GLM_ENABLE_EXPERIMENTAL
#include "GltfNode.h"
#include "Logger.h"
#include <algorithm>
#include <glm/gtx/matrix_decompose.hpp>

std::shared_ptr<GltfNode> GltfNode::createRoot(int nodeIndex) {
    auto node = std::make_shared<GltfNode>();
    node->mNodeIndex = nodeIndex;
    return node;
}

void GltfNode::calculateLocalTRSMatrix() {
    glm::mat4 sMatrix = glm::scale(glm::mat4(1.0f), mBlendScale);
    glm::mat4 rMatrix = glm::mat4_cast(mBlendRotation);
    glm::mat4 tMatrix = glm::translate(glm::mat4(1.0f), mBlendTranslation);
    mLocalTRSMatrix = tMatrix * rMatrix * sMatrix;
}

void GltfNode::blendScale(glm::vec3 scale, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    mBlendScale = scale * factor + mScale * (1.0f - factor);
}

void GltfNode::blendTranslation(glm::vec3 translation, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    mBlendTranslation = translation * factor + mTranslation * (1.0f - factor);
}

void GltfNode::blendRotation(glm::quat rotation, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    mBlendRotation = glm::normalize(glm::slerp(mRotation, rotation, factor));
}

void GltfNode::printTree(int indent) {
    if (indent == 0) {
        Logger::log(1, "printTree: ---- tree ----\n");
        Logger::log(1, "printTree: parent : %d (%s)\n", mNodeIndex, mName.c_str());
    }
    for (const auto& child : mChildNodes) {
        std::string indentStr = "";
        for (int i = 0; i < indent + 1; ++i) {
            indentStr += " ";
        }
        Logger::log(1, "printNodes:%s- child : %d (%s)\n", indentStr.c_str(), child->mNodeIndex, child->mName.c_str());
        child->printTree(indent + 1);
    }
    if (indent == 0) {
        Logger::log(1, "printTree: -- end tree --\n");
    }
}

std::shared_ptr<GltfNode> GltfNode::getParentNode() {
    std::shared_ptr<GltfNode> pNode = mParentNode.lock();
    if (pNode) {
        return pNode;
    }
    return nullptr;
}

glm::quat GltfNode::getLocalRotation() {
    return mBlendRotation;
}

glm::quat GltfNode::getGlobalRotation() {
    glm::quat orientation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;

    if (!glm::decompose(mNodeMatrix, scale, orientation, translation, skew, perspective)) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    return glm::inverse(orientation);
}

glm::vec3 GltfNode::getGlobalPosition() {
    glm::quat orientation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;

    if (!glm::decompose(mNodeMatrix, scale, orientation, translation, skew, perspective)) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return translation;
}

void GltfNode::updateNodeAndChildMatrices() {
    calculateNodeMatrix();
    for (auto &node : mChildNodes) {
        if (node) {
            node->updateNodeAndChildMatrices();
        }
    }
}

void GltfNode::calculateNodeMatrix() {
    calculateLocalTRSMatrix();
    glm::mat4 parentNodeMatrix = glm::mat4(1.0f);
    std::shared_ptr<GltfNode> pNode = mParentNode.lock();
    if (pNode) {
        parentNodeMatrix = pNode->mNodeMatrix;
    }
    mNodeMatrix = parentNodeMatrix * mLocalTRSMatrix;
}
