#include "GltfNode.h"
#include "Logger.h"

std::shared_ptr<GltfNode> GltfNode::createRoot(int nodeIndex) {
    auto node = std::make_shared<GltfNode>();
    node->mNodeIndex = nodeIndex;
    return node;
}

void GltfNode::calculateLocalTRSMatrix() {
    glm::mat4 sMatrix = glm::scale(glm::mat4(1.0f), mScale);
    glm::mat4 rMatrix = glm::mat4_cast(mRotation);
    glm::mat4 tMatrix = glm::translate(glm::mat4(1.0f), mTranslation);
    mLocalTRSMatrix = tMatrix * rMatrix * sMatrix;
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
