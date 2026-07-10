#pragma once
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class GltfNode {
public:
    int mNodeIndex = -1;
    std::string mName;

    glm::vec3 mScale = glm::vec3(1.0f);
    glm::vec3 mTranslation = glm::vec3(0.0f);
    glm::quat mRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    glm::mat4 mLocalTRSMatrix = glm::mat4(1.0f);
    glm::mat4 mNodeMatrix = glm::mat4(1.0f);
    glm::mat4 mInverseBindMatrix = glm::mat4(1.0f);

    std::vector<std::shared_ptr<GltfNode>> mChildNodes{};

    static std::shared_ptr<GltfNode> createRoot(int nodeIndex);
    void calculateLocalTRSMatrix();
    void printTree(int indent = 0);
};
