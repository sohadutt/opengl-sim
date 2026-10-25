#pragma once

#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/physics/light_particle_field.hpp"

#include <cstdint>
#include <glm/glm.hpp>

namespace opengl_sim::render {

// Owns the procedural GPU particle field independently from scene geometry.
class ParticleRenderer {
public:
    explicit ParticleRenderer(core::Logger& logger) noexcept;
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    [[nodiscard]] bool initialize();
    void draw(const physics::light_particle_field& particles,
              const physics::black_hole& black_hole,
              float scene_extent,
              const glm::mat4& view_projection,
              const glm::vec3& camera_position,
              bool depth_only = false) const noexcept;
    void release() noexcept;

private:
    [[nodiscard]] std::uint32_t compileShader(std::uint32_t type, const char* source);

    core::Logger& logger_;
    std::uint32_t program_ = 0;
    std::uint32_t vertex_array_ = 0;
    int view_projection_uniform_ = -1;
    int particle_size_uniform_ = -1;
    int trail_samples_uniform_ = -1;
    int particle_time_uniform_ = -1;
    int particle_speed_uniform_ = -1;
    int particle_extent_uniform_ = -1;
    int particle_horizon_uniform_ = -1;
    int particle_mass_uniform_ = -1;
    int particle_spin_uniform_ = -1;
    int black_hole_position_uniform_ = -1;
    int camera_position_uniform_ = -1;
    bool initialized_ = false;
};

} // namespace opengl_sim::render
