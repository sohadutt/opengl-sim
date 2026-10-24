#pragma once

#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/atom.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/physics/grid_plane.hpp"
#include "opengl_sim/physics/light_particle_field.hpp"
#include "opengl_sim/physics/planet.hpp"
#include "opengl_sim/render/camera.hpp"
#include "opengl_sim/render/atom_renderer.hpp"
#include "opengl_sim/render/particle_renderer.hpp"

#include <cstdint>
#include <vector>

namespace opengl_sim::render {

class Renderer {
public:
    explicit Renderer(core::Logger& logger) noexcept;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] bool initialize();
    void draw(const physics::black_hole& black_hole,
              const physics::grid_plane& grid, bool show_grid,
              const physics::light_particle_field& particles, const std::vector<physics::planet>& planets,
              const Camera& camera,
              int width, int height);
    void drawAtomic(const physics::atom& atom, const Camera& camera,
                    int width, int height);
    void drawSelection(int width, int height) const noexcept;

private:
    [[nodiscard]] std::uint32_t compileShader(std::uint32_t type, const char* source);
    [[nodiscard]] bool ensureRenderTarget(int width, int height);
    void release() noexcept;

    core::Logger& logger_;
    std::uint32_t program_ = 0;
    std::uint32_t lensing_program_ = 0;
    std::uint32_t vertex_array_ = 0;
    std::uint32_t lensing_vertex_array_ = 0;
    std::uint32_t vertex_buffer_ = 0;
    std::uint32_t framebuffer_ = 0;
    std::uint32_t scene_texture_ = 0;
    std::uint32_t depth_texture_ = 0;
    int render_target_width_ = 0;
    int render_target_height_ = 0;
    int color_uniform_ = -1;
    int view_projection_uniform_ = -1;
    int point_size_uniform_ = -1;
    int vertex_color_uniform_ = -1;
    int lighting_uniform_ = -1;
    int model_matrix_uniform_ = -1;
    int planet_color_uniform_ = -1;
    int planet_material_uniform_ = -1;
    int scene_texture_uniform_ = -1;
    int scene_depth_uniform_ = -1;
    int lensing_center_ndc_uniform_ = -1;
    int lensing_aspect_uniform_ = -1;
    int einstein_radius_uniform_ = -1;
    int shadow_radius_uniform_ = -1;
    int lens_depth_uniform_ = -1;
    int background_color_uniform_ = -1;
    int viewport_size_uniform_ = -1;
    ParticleRenderer particle_renderer_;
    AtomRenderer atom_renderer_;
    bool initialized_ = false;
};

} // namespace opengl_sim::render
