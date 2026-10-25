#pragma once

#include "opengl_sim/app/scene.hpp"
#include "opengl_sim/physics/atom.hpp"

namespace opengl_sim::app {

class AtomicScene final : public Scene {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "atomic"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "ATOMIC MODE"; }
    void configureUI(ui::UserInterface& interface) override;
    void update(double delta_seconds) override;
    void draw(render::Renderer& renderer, const render::Camera& camera,
              int width, int height) override;

private:
    void loadPreset(int preset) noexcept;

    physics::atom atom_;
    int selected_preset_ = 0;
};

} // namespace opengl_sim::app
