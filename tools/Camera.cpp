#include <glm/gtc/matrix_transform.hpp>
#include "Camera.h"

glm::mat4 Camera::getViewMatrix(VkRenderData &renderData) {
    float azimRad = glm::radians(renderData.rdViewAzimuth);
    float elevRad = glm::radians(renderData.rdViewElevation);

    float sinAzim = glm::sin(azimRad);
    float cosAzim = glm::cos(azimRad);
    float sinElev = glm::sin(elevRad);
    float cosElev = glm::cos(elevRad);

    mViewDirection = glm::normalize(glm::vec3(
        sinAzim * cosElev,
        sinElev,
       -cosAzim * cosElev
    ));

    mRightDirection = glm::normalize(glm::cross(mViewDirection, mWorldUpVector));
    mUpDirection = glm::normalize(glm::cross(mRightDirection, mViewDirection));

    float speed = 5.0f;
    renderData.rdCameraWorldPosition += renderData.rdMoveForward * renderData.rdTickDiff * speed * mViewDirection
                                      + renderData.rdMoveRight * renderData.rdTickDiff * speed * mRightDirection
                                      + renderData.rdMoveUp * renderData.rdTickDiff * speed * mUpDirection;

    return glm::lookAt(renderData.rdCameraWorldPosition, renderData.rdCameraWorldPosition + mViewDirection, mUpDirection);
}
