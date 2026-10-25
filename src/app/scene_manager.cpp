#include "opengl_sim/app/scene_manager.hpp"

#include <string>

namespace opengl_sim::app {

SceneManager::SceneManager(ui::UserInterface& interface, core::Logger& logger) noexcept
    : interface_(interface), logger_(logger) {}

void SceneManager::registerScene(std::unique_ptr<Scene> scene) {
    if (!scene) {
        logger_.warning("Ignoring a null scene registration.");
        return;
    }
    for (const auto& existing : scenes_) {
        if (existing->id() == scene->id()) {
            logger_.warning("Ignoring duplicate scene id '" + std::string{scene->id()} + "'.");
            return;
        }
    }
    scenes_.push_back(std::move(scene));
}

void SceneManager::requestScene(std::optional<std::size_t> scene_index) noexcept {
    requested_scene_ = scene_index;
    has_scene_request_ = true;
}

void SceneManager::configureUI() {
    interface_.clearControls();
    if (!active_scene_) {
        interface_.setPanelTitle("CHOOSE SCENE");
        interface_.addInfo([] { return "SELECT A SIMULATION"; });
        for (std::size_t index = 0; index < scenes_.size(); ++index) {
            const auto label = std::string{scenes_[index]->title()};
            interface_.addButton(label, [this, index] { requestScene(index); });
        }
        return;
    }

    auto& scene = scenes_[*active_scene_];
    interface_.setPanelTitle(std::string{scene->title()});
    scene->configureUI(interface_);
    interface_.addButton("BACK TO SCENES", [this] { requestScene(std::nullopt); });
}

void SceneManager::processSceneChange() {
    if (!has_scene_request_) return;
    if (requested_scene_ && *requested_scene_ >= scenes_.size()) {
        logger_.warning("Ignoring request for an unregistered scene.");
        has_scene_request_ = false;
        return;
    }
    active_scene_ = requested_scene_;
    has_scene_request_ = false;
    configureUI();
}

void SceneManager::update(double delta_seconds) {
    if (active_scene_) scenes_[*active_scene_]->update(delta_seconds);
}

void SceneManager::draw(render::Renderer& renderer, const render::Camera& camera,
                        int width, int height) {
    if (active_scene_) {
        scenes_[*active_scene_]->draw(renderer, camera, width, height);
    } else {
        renderer.drawSelection(width, height);
    }
}

} // namespace opengl_sim::app
