#pragma once

#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/render/camera.hpp"

#include <cstdint>

namespace opengl_sim::render {

class Renderer {
public:
    explicit Renderer(core::Logger& logger) noexcept;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] bool initialize();
    void draw(const physics::black_hole& black_hole, const Camera& camera, int width, int height);

private:
    [[nodiscard]] std::uint32_t compileShader(std::uint32_t type, const char* source);
    void release() noexcept;

    core::Logger& logger_;
    std::uint32_t program_ = 0;
    std::uint32_t vertex_array_ = 0;
    std::uint32_t vertex_buffer_ = 0;
    int color_uniform_ = -1;
    int view_projection_uniform_ = -1;
    bool initialized_ = false;
};

} // namespace opengl_sim::render
