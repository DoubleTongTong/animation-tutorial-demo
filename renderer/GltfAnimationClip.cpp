#include "GltfAnimationClip.h"
#include <algorithm>

GltfAnimationClip::GltfAnimationClip(std::string name) : mClipName(name) {}

void GltfAnimationClip::addChannel(
    std::shared_ptr<tinygltf::Model> model,
    tinygltf::Animation anim,
    tinygltf::AnimationChannel channel) {
    auto chan = std::make_shared<GltfAnimationChannel>();
    chan->loadChannelData(model, anim, channel);
    mAnimationChannels.push_back(chan);
}

void GltfAnimationClip::blendAnimationFrame(
    std::vector<std::shared_ptr<GltfNode>>& nodes,
    const std::vector<bool>& additiveMask,
    float time,
    float blendFactor) {
    for (auto &channel : mAnimationChannels) {
        int targetNode = channel->getTargetNode();
        if (targetNode < 0 || targetNode >= static_cast<int>(nodes.size()) || !nodes[targetNode]) {
            continue;
        }
        if (targetNode < static_cast<int>(additiveMask.size()) && !additiveMask[targetNode]) {
            continue;
        }

        switch (channel->getTargetPath()) {
            case ETargetPath::ROTATION:
                nodes.at(targetNode)->blendRotation(channel->getRotation(time), blendFactor);
                break;
            case ETargetPath::TRANSLATION:
                nodes.at(targetNode)->blendTranslation(channel->getTranslation(time), blendFactor);
                break;
            case ETargetPath::SCALE:
                nodes.at(targetNode)->blendScale(channel->getScaling(time), blendFactor);
                break;
        }
    }

    for (auto &node : nodes) {
        if (node) {
            node->calculateLocalTRSMatrix();
        }
    }
}

void GltfAnimationClip::setAnimationFrame(
    std::vector<std::shared_ptr<GltfNode>>& nodes,
    const std::vector<bool>& additiveMask,
    float time) {
    for (auto &channel : mAnimationChannels) {
        int targetNode = channel->getTargetNode();
        if (targetNode < 0 || targetNode >= static_cast<int>(nodes.size()) || !nodes[targetNode]) {
            continue;
        }
        if (targetNode < static_cast<int>(additiveMask.size()) && !additiveMask[targetNode]) {
            continue;
        }

        switch (channel->getTargetPath()) {
            case ETargetPath::ROTATION:
                nodes.at(targetNode)->setRotation(channel->getRotation(time));
                break;
            case ETargetPath::TRANSLATION:
                nodes.at(targetNode)->setTranslation(channel->getTranslation(time));
                break;
            case ETargetPath::SCALE:
                nodes.at(targetNode)->setScale(channel->getScaling(time));
                break;
        }
    }

    for (auto &node : nodes) {
        if (node) {
            node->calculateLocalTRSMatrix();
        }
    }
}

float GltfAnimationClip::getClipEndTime() const {
    float maxTime = 0.0f;
    for (const auto& channel : mAnimationChannels) {
        maxTime = std::max(maxTime, channel->getMaxTime());
    }
    return maxTime;
}

std::string GltfAnimationClip::getClipName() const {
    return mClipName;
}
