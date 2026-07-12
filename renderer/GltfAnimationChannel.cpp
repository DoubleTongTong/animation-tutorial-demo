#include "GltfAnimationChannel.h"
#include <cstring>
#include <algorithm>

void GltfAnimationChannel::loadChannelData(
    std::shared_ptr<tinygltf::Model> model,
    tinygltf::Animation anim,
    tinygltf::AnimationChannel channel) {

    mTargetNode = channel.target_node;

    // Load sampler
    const tinygltf::AnimationSampler& sampler = anim.samplers.at(channel.sampler);

    // Get Interpolation Type
    if (sampler.interpolation == "STEP") {
        mInterType = EInterpolationType::STEP;
    } else if (sampler.interpolation == "LINEAR") {
        mInterType = EInterpolationType::LINEAR;
    } else {
        mInterType = EInterpolationType::CUBICSPLINE;
    }

    // Load timings (Input accessor)
    const tinygltf::Accessor& inputAccessor = model->accessors.at(sampler.input);
    const tinygltf::BufferView& inputBufferView = model->bufferViews.at(inputAccessor.bufferView);
    const tinygltf::Buffer& inputBuffer = model->buffers.at(inputBufferView.buffer);

    std::vector<float> timings;
    timings.resize(inputAccessor.count);
    std::memcpy(timings.data(),
                inputBuffer.data.data() + inputBufferView.byteOffset + inputAccessor.byteOffset,
                inputAccessor.count * sizeof(float));
    setTimings(timings);

    // Load keyframe data (Output accessor)
    const tinygltf::Accessor& outputAccessor = model->accessors.at(sampler.output);
    const tinygltf::BufferView& outputBufferView = model->bufferViews.at(outputAccessor.bufferView);
    const tinygltf::Buffer& outputBuffer = model->buffers.at(outputBufferView.buffer);

    if (channel.target_path == "rotation") {
        mTargetPath = ETargetPath::ROTATION;
        std::vector<glm::quat> rotations;
        rotations.resize(outputAccessor.count);
        std::memcpy(rotations.data(),
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                    outputAccessor.count * sizeof(glm::quat));
        setRotations(rotations);
    } else if (channel.target_path == "translation") {
        mTargetPath = ETargetPath::TRANSLATION;
        std::vector<glm::vec3> translations;
        translations.resize(outputAccessor.count);
        std::memcpy(translations.data(),
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                    outputAccessor.count * sizeof(glm::vec3));
        setTranslations(translations);
    } else {
        mTargetPath = ETargetPath::SCALE;
        std::vector<glm::vec3> scalings;
        scalings.resize(outputAccessor.count);
        std::memcpy(scalings.data(),
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                    outputAccessor.count * sizeof(glm::vec3));
        setScalings(scalings);
    }
}

int GltfAnimationChannel::getTargetNode() {
    return mTargetNode;
}

ETargetPath GltfAnimationChannel::getTargetPath() {
    return mTargetPath;
}

float GltfAnimationChannel::getMaxTime() {
    if (mTimings.empty()) return 0.0f;
    return mTimings.back();
}

void GltfAnimationChannel::setTimings(const std::vector<float>& timings) {
    mTimings = timings;
}

void GltfAnimationChannel::setScalings(const std::vector<glm::vec3>& scalings) {
    mScaling = scalings;
}

void GltfAnimationChannel::setTranslations(const std::vector<glm::vec3>& translations) {
    mTranslations = translations;
}

void GltfAnimationChannel::setRotations(const std::vector<glm::quat>& rotations) {
    mRotations = rotations;
}

glm::vec3 GltfAnimationChannel::getScaling(float time) {
    if (mScaling.empty()) {
        return glm::vec3(1.0f);
    }
    if (time < mTimings.front()) {
        return mScaling.front();
    }
    if (time > mTimings.back()) {
        return mScaling.back();
    }

    int prevTimeIndex = 0;
    int nextTimeIndex = 0;
    for (size_t i = 0; i < mTimings.size(); ++i) {
        if (mTimings.at(i) > time) {
            nextTimeIndex = static_cast<int>(i);
            break;
        }
        prevTimeIndex = static_cast<int>(i);
    }

    if (prevTimeIndex == nextTimeIndex) {
        return mScaling.at(prevTimeIndex);
    }

    glm::vec3 finalScale = glm::vec3(1.0f);
    switch (mInterType) {
        case EInterpolationType::STEP:
            finalScale = mScaling.at(prevTimeIndex);
            break;
        case EInterpolationType::LINEAR: {
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            glm::vec3 prevVal = mScaling.at(prevTimeIndex);
            glm::vec3 nextVal = mScaling.at(nextTimeIndex);
            finalScale = prevVal + t * (nextVal - prevVal);
            break;
        }
        case EInterpolationType::CUBICSPLINE: {
            float deltaTime = mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex);
            glm::vec3 prevTangent = deltaTime * mScaling.at(prevTimeIndex * 3 + 2);
            glm::vec3 nextTangent = deltaTime * mScaling.at(nextTimeIndex * 3);
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            float t2 = t * t;
            float t3 = t2 * t;
            glm::vec3 prevPoint = mScaling.at(prevTimeIndex * 3 + 1);
            glm::vec3 nextPoint = mScaling.at(nextTimeIndex * 3 + 1);
            finalScale = (2.0f * t3 - 3.0f * t2 + 1.0f) * prevPoint +
                         (t3 - 2.0f * t2 + t) * prevTangent +
                         (-2.0f * t3 + 3.0f * t2) * nextPoint +
                         (t3 - t2) * nextTangent;
            break;
        }
    }
    return finalScale;
}

glm::vec3 GltfAnimationChannel::getTranslation(float time) {
    if (mTranslations.empty()) {
        return glm::vec3(0.0f);
    }
    if (time < mTimings.front()) {
        return mTranslations.front();
    }
    if (time > mTimings.back()) {
        return mTranslations.back();
    }

    int prevTimeIndex = 0;
    int nextTimeIndex = 0;
    for (size_t i = 0; i < mTimings.size(); ++i) {
        if (mTimings.at(i) > time) {
            nextTimeIndex = static_cast<int>(i);
            break;
        }
        prevTimeIndex = static_cast<int>(i);
    }

    if (prevTimeIndex == nextTimeIndex) {
        return mTranslations.at(prevTimeIndex);
    }

    glm::vec3 finalTrans = glm::vec3(0.0f);
    switch (mInterType) {
        case EInterpolationType::STEP:
            finalTrans = mTranslations.at(prevTimeIndex);
            break;
        case EInterpolationType::LINEAR: {
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            glm::vec3 prevVal = mTranslations.at(prevTimeIndex);
            glm::vec3 nextVal = mTranslations.at(nextTimeIndex);
            finalTrans = prevVal + t * (nextVal - prevVal);
            break;
        }
        case EInterpolationType::CUBICSPLINE: {
            float deltaTime = mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex);
            glm::vec3 prevTangent = deltaTime * mTranslations.at(prevTimeIndex * 3 + 2);
            glm::vec3 nextTangent = deltaTime * mTranslations.at(nextTimeIndex * 3);
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            float t2 = t * t;
            float t3 = t2 * t;
            glm::vec3 prevPoint = mTranslations.at(prevTimeIndex * 3 + 1);
            glm::vec3 nextPoint = mTranslations.at(nextTimeIndex * 3 + 1);
            finalTrans = (2.0f * t3 - 3.0f * t2 + 1.0f) * prevPoint +
                         (t3 - 2.0f * t2 + t) * prevTangent +
                         (-2.0f * t3 + 3.0f * t2) * nextPoint +
                         (t3 - t2) * nextTangent;
            break;
        }
    }
    return finalTrans;
}

glm::quat GltfAnimationChannel::getRotation(float time) {
    if (mRotations.empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (time < mTimings.front()) {
        return mRotations.front();
    }
    if (time > mTimings.back()) {
        return mRotations.back();
    }

    int prevTimeIndex = 0;
    int nextTimeIndex = 0;
    for (size_t i = 0; i < mTimings.size(); ++i) {
        if (mTimings.at(i) > time) {
            nextTimeIndex = static_cast<int>(i);
            break;
        }
        prevTimeIndex = static_cast<int>(i);
    }

    if (prevTimeIndex == nextTimeIndex) {
        return mRotations.at(prevTimeIndex);
    }

    glm::quat finalRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    switch (mInterType) {
        case EInterpolationType::STEP:
            finalRot = mRotations.at(prevTimeIndex);
            break;
        case EInterpolationType::LINEAR: {
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            glm::quat prevVal = mRotations.at(prevTimeIndex);
            glm::quat nextVal = mRotations.at(nextTimeIndex);
            finalRot = glm::normalize(glm::slerp(prevVal, nextVal, t));
            break;
        }
        case EInterpolationType::CUBICSPLINE: {
            float deltaTime = mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex);
            glm::quat prevTangent = deltaTime * mRotations.at(prevTimeIndex * 3 + 2);
            glm::quat nextTangent = deltaTime * mRotations.at(nextTimeIndex * 3);
            float t = (time - mTimings.at(prevTimeIndex)) / (mTimings.at(nextTimeIndex) - mTimings.at(prevTimeIndex));
            float t2 = t * t;
            float t3 = t2 * t;
            glm::quat prevPoint = mRotations.at(prevTimeIndex * 3 + 1);
            glm::quat nextPoint = mRotations.at(nextTimeIndex * 3 + 1);
            glm::quat rawRot = (2.0f * t3 - 3.0f * t2 + 1.0f) * prevPoint +
                               (t3 - 2.0f * t2 + t) * prevTangent +
                               (-2.0f * t3 + 3.0f * t2) * nextPoint +
                               (t3 - t2) * nextTangent;
            finalRot = glm::normalize(rawRot);
            break;
        }
    }
    return finalRot;
}
