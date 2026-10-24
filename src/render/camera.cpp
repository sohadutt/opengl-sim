#include "opengl_sim/render/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace opengl_sim::render {

Camera::Camera() noexcept { updateDirection(); }

void Camera::move(glm::vec3 direction, double delta_seconds) noexcept {
    if (delta_seconds <= 0.0) return;

    const auto magnitude = glm::length(direction);
    if (magnitude > 1.0F) direction /= magnitude;
    const auto right = glm::normalize(glm::cross(front_, world_up_));
    const auto movement = front_ * direction.z + world_up_ * direction.y + right * direction.x;
    position_ += movement * (movement_speed_ * static_cast<float>(delta_seconds));
}

void Camera::adjustZoom(double amount, double delta_seconds) noexcept {
    if (delta_seconds <= 0.0) return;
    const auto next_fov = field_of_view_degrees_ -
                          static_cast<float>(amount * delta_seconds * 30.0);
    field_of_view_degrees_ = std::clamp(next_fov, 20.0F, 80.0F);
}

void Camera::zoomFromScroll(double amount) noexcept {
    field_of_view_degrees_ = std::clamp(
        field_of_view_degrees_ - static_cast<float>(amount * 2.0), 20.0F, 80.0F);
}

void Camera::processMouseMovement(double x_position, double y_position) noexcept {
    if (first_mouse_event_) {
        last_mouse_x_ = x_position;
        last_mouse_y_ = y_position;
        first_mouse_event_ = false;
        return;
    }

    constexpr float sensitivity = 0.08F;
    const auto x_offset = static_cast<float>(x_position - last_mouse_x_) * sensitivity;
    const auto y_offset = static_cast<float>(last_mouse_y_ - y_position) * sensitivity;
    last_mouse_x_ = x_position;
    last_mouse_y_ = y_position;

    yaw_degrees_ += x_offset;
    pitch_degrees_ = std::clamp(pitch_degrees_ + y_offset, -89.0F, 89.0F);
    updateDirection();
}

void Camera::updateDirection() noexcept {
    const auto yaw = glm::radians(yaw_degrees_);
    const auto pitch = glm::radians(pitch_degrees_);
    front_ = glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch),
                                      std::sin(pitch),
                                      std::sin(yaw) * std::cos(pitch)});
}

glm::mat4 Camera::viewMatrix() const noexcept {
    return glm::lookAt(position_, position_ + front_, world_up_);
}

glm::mat4 Camera::projectionMatrix(float aspect_ratio) const noexcept {
    return glm::perspective(glm::radians(field_of_view_degrees_),
                            std::max(aspect_ratio, 0.0001F), 0.05F, 100.0F);
}

} // namespace opengl_sim::render
