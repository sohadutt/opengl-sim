#pragma once

#include "opengl_sim/render/camera.hpp"
#include "opengl_sim/render/renderer.hpp"
#include "opengl_sim/ui/user_interface.hpp"

#include <string_view>

namespace opengl_sim::app {

// A scene owns its simulation state and binds its own UI. Application-level
// windowing and input remain shared across every scene.
class Scene {
public:
    virtual ~Scene() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view title() const noexcept = 0;
    virtual void configureUI(ui::UserInterface& interface) = 0;
    virtual void update(double delta_seconds) = 0;
    virtual void draw(render::Renderer& renderer, const render::Camera& camera,
                      int width, int height) = 0;
};

} // namespace opengl_sim::app
