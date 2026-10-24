#include "opengl_sim/render/renderer.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

namespace opengl_sim::render {
namespace {

constexpr char vertex_shader_source[] = R"glsl(
#version 410 core
layout (location = 0) in vec3 position;
uniform mat4 view_projection;
uniform vec4 draw_color;
out vec4 fragment_color;
void main() {
    gl_Position = view_projection * vec4(position, 1.0);
    fragment_color = draw_color;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(
#version 410 core
in vec4 fragment_color;
out vec4 output_color;
void main() {
    output_color = fragment_color;
}
)glsl";

struct Vertex { float x; float y; float z; };

} // namespace

Renderer::Renderer(core::Logger& logger) noexcept : logger_(logger) {}
Renderer::~Renderer() { release(); }

std::uint32_t Renderer::compileShader(std::uint32_t type, const char* source) {
    const auto shader = glCreateShader(static_cast<GLenum>(type));
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, message.data());
        logger_.error("OpenGL shader compilation failed: " + message);
        glDeleteShader(shader);
        return 0;
    }
    return static_cast<std::uint32_t>(shader);
}

bool Renderer::initialize() {
#if !defined(__APPLE__)
    glewExperimental = GL_TRUE;
    const auto status = glewInit();
    if (status != GLEW_OK) {
        logger_.critical("Could not initialize GLEW: " +
                         std::string(reinterpret_cast<const char*>(glewGetErrorString(status))));
        return false;
    }
    static_cast<void>(glGetError());
#endif

    const auto vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_shader_source);
    const auto fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(static_cast<GLuint>(vertex_shader));
        if (fragment_shader != 0) glDeleteShader(static_cast<GLuint>(fragment_shader));
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(static_cast<GLuint>(program_), static_cast<GLuint>(vertex_shader));
    glAttachShader(static_cast<GLuint>(program_), static_cast<GLuint>(fragment_shader));
    glLinkProgram(static_cast<GLuint>(program_));
    glDeleteShader(static_cast<GLuint>(vertex_shader));
    glDeleteShader(static_cast<GLuint>(fragment_shader));

    GLint linked = GL_FALSE;
    glGetProgramiv(static_cast<GLuint>(program_), GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(static_cast<GLuint>(program_), GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(static_cast<GLuint>(program_), length, nullptr, message.data());
        logger_.critical("OpenGL shader program linking failed: " + message);
        release();
        return false;
    }

    color_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "draw_color");
    view_projection_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "view_projection");
    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    vertex_array_ = vertex_array;
    vertex_buffer_ = vertex_buffer;
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    initialized_ = true;
    logger_.info("OpenGL renderer initialized.");
    return true;
}

void Renderer::draw(const physics::black_hole& black_hole, const Camera& camera,
                    int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.012F, 0.018F, 0.035F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(static_cast<GLuint>(program_));
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vertex_buffer_));

    const auto aspect = static_cast<float>(width) / static_cast<float>(height);
    const auto view_projection = camera.projectionMatrix(aspect) * camera.viewMatrix();
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, glm::value_ptr(view_projection));
    const auto upload_and_draw = [this](const std::vector<Vertex>& vertices, GLenum mode,
                                        const std::array<float, 4>& color) {
        if (vertices.empty()) return;
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STREAM_DRAW);
        glUniform4fv(color_uniform_, 1, color.data());
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
    };

    // A ground plane grid makes depth and camera motion visible in the scene.
    std::vector<Vertex> grid;
    constexpr int grid_extent = 10;
    constexpr float grid_spacing = 0.5F;
    constexpr float grid_height = -0.48F;
    grid.reserve(84 * 2);
    for (int i = -grid_extent * 2; i <= grid_extent * 2; ++i) {
        const auto coordinate = static_cast<float>(i) * grid_spacing;
        grid.push_back({coordinate, grid_height, -static_cast<float>(grid_extent)});
        grid.push_back({coordinate, grid_height, static_cast<float>(grid_extent)});
        grid.push_back({-static_cast<float>(grid_extent), grid_height, coordinate});
        grid.push_back({static_cast<float>(grid_extent), grid_height, coordinate});
    }
    upload_and_draw(grid, GL_LINES, {0.10F, 0.18F, 0.25F, 1.0F});

    const std::vector<Vertex> axes{
        {-static_cast<float>(grid_extent), grid_height + 0.002F, 0.0F},
        { static_cast<float>(grid_extent), grid_height + 0.002F, 0.0F},
        {0.0F, grid_height + 0.002F, -static_cast<float>(grid_extent)},
        {0.0F, grid_height + 0.002F,  static_cast<float>(grid_extent)},
        {0.0F, grid_height, 0.0F}, {0.0F, 1.5F, 0.0F},
    };
    upload_and_draw(std::vector<Vertex>{axes.begin(), axes.begin() + 2}, GL_LINES,
                    {0.75F, 0.20F, 0.18F, 1.0F});
    upload_and_draw(std::vector<Vertex>{axes.begin() + 2, axes.begin() + 4}, GL_LINES,
                    {0.20F, 0.45F, 0.85F, 1.0F});
    upload_and_draw(std::vector<Vertex>{axes.begin() + 4, axes.end()}, GL_LINES,
                    {0.25F, 0.78F, 0.40F, 1.0F});

    const auto outline = black_hole.event_horizon_outline(160);
    const auto vertex_at = [](double x, double y) {
        return Vertex{static_cast<float>(x), static_cast<float>(y), 0.0F};
    };

    const std::array bands{
        std::tuple{1.10, 1.42, std::array<float, 4>{0.92F, 0.29F, 0.055F, 1.0F}},
        std::tuple{1.015, 1.10, std::array<float, 4>{1.0F, 0.68F, 0.22F, 1.0F}},
    };
    for (const auto& [inner_scale, outer_scale, color] : bands) {
        std::vector<Vertex> vertices;
        vertices.reserve(outline.size() * 2);
        for (const auto& point : outline) {
            const auto dx = point.x - black_hole.position.x;
            const auto dy = point.y - black_hole.position.y;
            vertices.push_back(vertex_at(black_hole.position.x + dx * outer_scale,
                                         black_hole.position.y + dy * outer_scale));
            vertices.push_back(vertex_at(black_hole.position.x + dx * inner_scale,
                                         black_hole.position.y + dy * inner_scale));
        }
        upload_and_draw(vertices, GL_TRIANGLE_STRIP, color);
    }

    std::vector<Vertex> disk;
    disk.reserve(outline.size() + 1);
    disk.push_back(vertex_at(black_hole.position.x, black_hole.position.y));
    for (const auto& point : outline) disk.push_back(vertex_at(point.x, point.y));
    upload_and_draw(disk, GL_TRIANGLE_FAN, {0.002F, 0.003F, 0.008F, 1.0F});

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::release() noexcept {
    const auto buffer = static_cast<GLuint>(vertex_buffer_);
    const auto array = static_cast<GLuint>(vertex_array_);
    if (buffer != 0) glDeleteBuffers(1, &buffer);
    if (array != 0) glDeleteVertexArrays(1, &array);
    if (program_ != 0) glDeleteProgram(static_cast<GLuint>(program_));
    vertex_buffer_ = 0;
    vertex_array_ = 0;
    program_ = 0;
    initialized_ = false;
}

} // namespace opengl_sim::render
