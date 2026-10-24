#pragma once

#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/atom.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace opengl_sim::render {

// Renders Monte Carlo samples from spherically averaged atomic orbital density.
class AtomRenderer {
public:
    explicit AtomRenderer(core::Logger& logger) noexcept;
    ~AtomRenderer();

    AtomRenderer(const AtomRenderer&) = delete;
    AtomRenderer& operator=(const AtomRenderer&) = delete;

    [[nodiscard]] bool initialize();
    void draw(const physics::atom& atom, const glm::mat4& view_projection);
    void release() noexcept;

private:
    [[nodiscard]] std::uint32_t compileShader(std::uint32_t type, const char* source);
    void updateProbabilityTexture(const physics::atom& atom);

    core::Logger& logger_;
    std::uint32_t program_ = 0;
    std::uint32_t vertex_array_ = 0;
    std::uint32_t probability_texture_ = 0;
    int view_projection_uniform_ = -1;
    int probability_texture_uniform_ = -1;
    int orbital_count_uniform_ = -1;
    int cumulative_occupancy_uniform_ = -1;
    int max_radius_uniform_ = -1;
    int nucleus_pass_uniform_ = -1;
    int proton_count_uniform_ = -1;
    int neutron_count_uniform_ = -1;
    int last_proton_count_ = -1;
    int last_neutron_count_ = -1;
    int last_electron_count_ = -1;
    double cached_max_radius_bohr_ = 1.0;
    std::vector<physics::atomic_orbital> cached_orbitals_;
    bool initialized_ = false;
};

} // namespace opengl_sim::render
