#pragma once

#include "opengl_sim/app/scene.hpp"
#include "opengl_sim/core/logger.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace opengl_sim::app {

class SceneManager {
public:
    SceneManager(ui::UserInterface& interface, core::Logger& logger) noexcept;

    void registerScene(std::unique_ptr<Scene> scene);
    void configureUI();
    void processSceneChange();
    void update(double delta_seconds);
    void draw(render::Renderer& renderer, const render::Camera& camera,
              int width, int height);

private:
    void requestScene(std::optional<std::size_t> scene_index) noexcept;

    ui::UserInterface& interface_;
    core::Logger& logger_;
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::optional<std::size_t> active_scene_;
    std::optional<std::size_t> requested_scene_;
    bool has_scene_request_ = false;
};

} // namespace opengl_sim::app
