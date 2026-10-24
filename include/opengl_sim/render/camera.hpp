#pragma once

#include <glm/glm.hpp>

namespace opengl_sim::render {

class Camera {
public:
    Camera() noexcept;

    void move(glm::vec3 direction, double delta_seconds) noexcept;
    void adjustZoom(double amount, double delta_seconds) noexcept;
    void zoomFromScroll(double amount) noexcept;
    void processMouseMovement(double x_position, double y_position) noexcept;
    void resetMouseTracking() noexcept { first_mouse_event_ = true; }

    [[nodiscard]] glm::mat4 viewMatrix() const noexcept;
    [[nodiscard]] glm::mat4 projectionMatrix(float aspect_ratio) const noexcept;
    [[nodiscard]] glm::vec3 position() const noexcept { return position_; }

private:
    void updateDirection() noexcept;

    glm::vec3 position_{0.0F, 0.7F, 4.5F};
    glm::vec3 front_{0.0F, 0.0F, -1.0F};
    glm::vec3 world_up_{0.0F, 1.0F, 0.0F};
    float yaw_degrees_ = -90.0F;
    float pitch_degrees_ = -9.0F;
    float field_of_view_degrees_ = 55.0F;
    float movement_speed_ = 3.0F;
    double last_mouse_x_ = 0.0;
    double last_mouse_y_ = 0.0;
    bool first_mouse_event_ = true;
};

} // namespace opengl_sim::render
