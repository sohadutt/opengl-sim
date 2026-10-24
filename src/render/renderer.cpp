#include "opengl_sim/render/renderer.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <vector>

namespace opengl_sim::render {
namespace {

constexpr char vertex_shader_source[] = R"glsl(
#version 410 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 vertex_color;
layout (location = 2) in vec3 vertex_normal;
uniform mat4 view_projection;
uniform mat4 model_matrix;
uniform vec4 draw_color;
uniform float point_size;
uniform bool use_vertex_color;
uniform bool use_lighting;
out vec4 fragment_color;
out vec3 surface_normal;
out float apply_lighting;
void main() {
    vec3 world_position = (model_matrix * vec4(position, 1.0)).xyz;
    gl_Position = view_projection * vec4(world_position, 1.0);
    gl_PointSize = point_size;
    fragment_color = use_vertex_color ? vertex_color : draw_color;
    surface_normal = mat3(model_matrix) * vertex_normal;
    apply_lighting = use_lighting ? 1.0 : 0.0;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(
#version 410 core
in vec4 fragment_color;
in vec3 surface_normal;
in float apply_lighting;
uniform bool planet_material;
uniform vec3 planet_base_color;
out vec4 output_color;
void main() {
    float diffuse = max(dot(normalize(surface_normal), normalize(vec3(-0.35, 0.55, 0.9))), 0.0);
    float light = mix(1.0, 0.16 + 0.84 * diffuse, apply_lighting);
    vec3 surface_color = fragment_color.rgb;
    if (planet_material) {
        surface_color = planet_base_color;
    }
    output_color = vec4(surface_color * light, fragment_color.a);
}
)glsl";

constexpr char post_vertex_shader_source[] = R"glsl(
#version 410 core
out vec2 texture_coordinates;
void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    vec2 position = positions[gl_VertexID];
    texture_coordinates = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)glsl";

constexpr char lensing_fragment_shader_source[] = R"glsl(
#version 410 core
in vec2 texture_coordinates;
uniform sampler2D scene_texture;
uniform sampler2D scene_depth;
uniform vec2 lens_center_ndc;
uniform float viewport_aspect;
uniform float einstein_radius_ndc;
uniform float shadow_radius_ndc;
uniform float lens_depth;
uniform vec3 background_color;
uniform vec2 viewport_size;
out vec4 output_color;
void main() {
    vec2 screen_coordinates = gl_FragCoord.xy / viewport_size;
    vec2 output_ndc = texture_coordinates * 2.0 - 1.0;
    vec2 theta = (output_ndc - lens_center_ndc) * vec2(viewport_aspect, 1.0);
    float theta_squared = dot(theta, theta);
    float safe_radius_squared = max(theta_squared, 0.000001);

    // Back-project the camera ray with the point-mass lens equation:
    // beta = theta - theta_E^2 * theta / |theta|^2.
    vec2 beta = theta * (1.0 -
        (einstein_radius_ndc * einstein_radius_ndc) / safe_radius_squared);
    vec2 source_ndc = lens_center_ndc + beta / vec2(viewport_aspect, 1.0);
    vec2 source_coordinates = source_ndc * 0.5 + 0.5;

    vec3 incoming_light = background_color;
    float direct_depth = texture(scene_depth, screen_coordinates).r;
    if (direct_depth < lens_depth) {
        // A surface in front of the lens lies directly on the camera ray.
        incoming_light = texture(scene_texture, screen_coordinates).rgb;
    } else if (all(greaterThanEqual(source_coordinates, vec2(0.0))) &&
               all(lessThanEqual(source_coordinates, vec2(1.0)))) {
        float source_depth = texture(scene_depth, source_coordinates).r;
        if (source_depth >= lens_depth) {
            incoming_light = texture(scene_texture, source_coordinates).rgb;
        }
    }

    // The black shadow blocks every ray whose impact parameter falls inside it.
    float impact_parameter = length(theta);
    float shadow_edge = smoothstep(shadow_radius_ndc * 0.985,
                                   shadow_radius_ndc * 1.015, impact_parameter);
    output_color = vec4(incoming_light * shadow_edge, 1.0);
}
)glsl";

struct Vertex {
    float x;
    float y;
    float z;
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;
    float normal_x = 0.0F;
    float normal_y = 0.0F;
    float normal_z = 0.0F;
};

const std::vector<Vertex>& unitSphereMesh() {
    constexpr int latitude_steps = 32;
    constexpr int longitude_steps = 64;
    static const auto mesh = [] {
        std::vector<Vertex> vertices;
        vertices.reserve(static_cast<std::size_t>(latitude_steps * longitude_steps * 6));
        const auto vertex = [](int latitude, int longitude) {
            const auto theta = -std::numbers::pi_v<double> / 2.0 +
                               std::numbers::pi_v<double> * latitude / latitude_steps;
            const auto phi = 2.0 * std::numbers::pi_v<double> * longitude / longitude_steps;
            const auto nx = std::cos(theta) * std::cos(phi);
            const auto ny = std::sin(theta);
            const auto nz = std::cos(theta) * std::sin(phi);
            return Vertex{static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz),
                          1.0F, 1.0F, 1.0F, 1.0F,
                          static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)};
        };
        for (int latitude = 0; latitude < latitude_steps; ++latitude) {
            for (int longitude = 0; longitude < longitude_steps; ++longitude) {
                const auto a = vertex(latitude, longitude);
                const auto b = vertex(latitude + 1, longitude);
                const auto c = vertex(latitude + 1, longitude + 1);
                const auto d = vertex(latitude, longitude + 1);
                vertices.insert(vertices.end(), {a, b, c, a, c, d});
            }
        }
        return vertices;
    }();
    return mesh;
}

} // namespace

Renderer::Renderer(core::Logger& logger) noexcept
    : logger_(logger), particle_renderer_(logger), atom_renderer_(logger) {}
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
    logger_.info("Initializing the OpenGL scene renderer.");
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

    if (!particle_renderer_.initialize()) return false;
    logger_.info("GPU light-particle renderer initialized.");
    if (!atom_renderer_.initialize()) return false;
    logger_.info("GPU atomic probability renderer initialized.");

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

    const auto post_vertex_shader = compileShader(GL_VERTEX_SHADER, post_vertex_shader_source);
    const auto post_fragment_shader = compileShader(GL_FRAGMENT_SHADER, lensing_fragment_shader_source);
    if (post_vertex_shader == 0 || post_fragment_shader == 0) {
        if (post_vertex_shader != 0) glDeleteShader(static_cast<GLuint>(post_vertex_shader));
        if (post_fragment_shader != 0) glDeleteShader(static_cast<GLuint>(post_fragment_shader));
        release();
        return false;
    }

    lensing_program_ = glCreateProgram();
    glAttachShader(static_cast<GLuint>(lensing_program_), static_cast<GLuint>(post_vertex_shader));
    glAttachShader(static_cast<GLuint>(lensing_program_), static_cast<GLuint>(post_fragment_shader));
    glLinkProgram(static_cast<GLuint>(lensing_program_));
    glDeleteShader(static_cast<GLuint>(post_vertex_shader));
    glDeleteShader(static_cast<GLuint>(post_fragment_shader));

    GLint post_linked = GL_FALSE;
    glGetProgramiv(static_cast<GLuint>(lensing_program_), GL_LINK_STATUS, &post_linked);
    if (post_linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(static_cast<GLuint>(lensing_program_), GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(static_cast<GLuint>(lensing_program_), length, nullptr, message.data());
        logger_.critical("Lensing shader program linking failed: " + message);
        release();
        return false;
    }

    color_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "draw_color");
    view_projection_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "view_projection");
    point_size_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "point_size");
    vertex_color_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "use_vertex_color");
    lighting_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "use_lighting");
    model_matrix_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "model_matrix");
    planet_color_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "planet_base_color");
    planet_material_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "planet_material");
    scene_texture_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "scene_texture");
    scene_depth_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "scene_depth");
    lensing_center_ndc_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "lens_center_ndc");
    lensing_aspect_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "viewport_aspect");
    einstein_radius_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "einstein_radius_ndc");
    shadow_radius_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "shadow_radius_ndc");
    lens_depth_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "lens_depth");
    background_color_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "background_color");
    viewport_size_uniform_ = glGetUniformLocation(static_cast<GLuint>(lensing_program_), "viewport_size");
    GLuint vertex_array = 0;
    GLuint lensing_vertex_array = 0;
    GLuint vertex_buffer = 0;
    glGenVertexArrays(1, &vertex_array);
    glGenVertexArrays(1, &lensing_vertex_array);
    glGenBuffers(1, &vertex_buffer);
    vertex_array_ = vertex_array;
    lensing_vertex_array_ = lensing_vertex_array;
    vertex_buffer_ = vertex_buffer;
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, red)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal_x)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint framebuffer = 0;
    GLuint scene_texture = 0;
    GLuint depth_texture = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &scene_texture);
    glGenTextures(1, &depth_texture);
    framebuffer_ = framebuffer;
    scene_texture_ = scene_texture;
    depth_texture_ = depth_texture;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    initialized_ = true;
    logger_.info("OpenGL renderer initialized.");
    return true;
}

bool Renderer::ensureRenderTarget(int width, int height) {
    if (width == render_target_width_ && height == render_target_height_) return true;

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(scene_texture_));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(depth_texture_));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           static_cast<GLuint>(scene_texture_), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           static_cast<GLuint>(depth_texture_), 0);
    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        logger_.critical("Could not create the scene framebuffer (status " +
                         std::to_string(status) + ").");
        return false;
    }

    render_target_width_ = width;
    render_target_height_ = height;
    return true;
}

void Renderer::draw(const physics::black_hole& black_hole,
                    const physics::grid_plane& grid_plane, bool show_grid,
                    const physics::light_particle_field& particles, const std::vector<physics::planet>& planets,
                    const Camera& camera,
                    int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0 || !ensureRenderTarget(width, height)) return;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
    glViewport(0, 0, width, height);
    glClearColor(0.012F, 0.018F, 0.035F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(static_cast<GLuint>(program_));
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vertex_buffer_));
    glUniform1f(point_size_uniform_, 1.0F);

    const auto aspect = static_cast<float>(width) / static_cast<float>(height);
    const auto projection = camera.projectionMatrix(aspect);
    const auto view_projection = projection * camera.viewMatrix();
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, glm::value_ptr(view_projection));
    const auto identity = glm::mat4{1.0F};
    glUniformMatrix4fv(model_matrix_uniform_, 1, GL_FALSE, glm::value_ptr(identity));
    const auto lens_center_world = glm::vec3{static_cast<float>(black_hole.position.x),
                                              static_cast<float>(black_hole.position.y),
                                              static_cast<float>(black_hole.position.z)};
    const auto lens_center_clip = view_projection * glm::vec4{lens_center_world, 1.0F};
    const auto lens_center_ndc = glm::vec2{lens_center_clip.x, lens_center_clip.y} /
                                 (std::abs(lens_center_clip.w) > 0.0001F
                                      ? lens_center_clip.w : 0.0001F);
    const auto camera_position = camera.position();
    const auto lens_distance = glm::length(camera_position - lens_center_world);
    float source_distance = lens_distance;
    for (const auto& planet : planets) {
        const auto distance = glm::length(camera_position - glm::vec3{
            static_cast<float>(planet.position.x), static_cast<float>(planet.position.y),
            static_cast<float>(planet.position.z)});
        if (distance > lens_distance + 0.01F) source_distance = std::max(source_distance, distance);
    }
    const auto source_behind_lens = source_distance > lens_distance + 0.01F &&
                                    lens_center_clip.w > 0.0F;
    const auto horizon = static_cast<float>(std::max(black_hole.event_horizon, 0.01));
    const auto lens_to_source = std::max(source_distance - lens_distance, 0.0F);
    const auto einstein_angle = source_behind_lens
        ? std::sqrt(2.0F * horizon * std::max(static_cast<float>(black_hole.mass), 0.0F) *
                    lens_to_source /
                    std::max(lens_distance * source_distance, 0.0001F))
        : 0.0F;
    const auto einstein_radius = einstein_angle * projection[1][1];
    glUniform1i(planet_material_uniform_, GL_FALSE);
    const auto upload_and_draw = [this](const std::vector<Vertex>& vertices, GLenum mode,
                                        const std::array<float, 4>& color,
                                        bool vertex_colored = false, bool lit = false) {
        if (vertices.empty()) return;
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STREAM_DRAW);
        glUniform4fv(color_uniform_, 1, color.data());
        glUniform1i(vertex_color_uniform_, vertex_colored ? GL_TRUE : GL_FALSE);
        glUniform1i(lighting_uniform_, lit ? GL_TRUE : GL_FALSE);
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
    };

    constexpr double grid_height = -0.48;
    const auto base_grid_height = grid_height + grid_plane.position.y;
    const auto bent_height = [&black_hole, base_grid_height](double x, double z) {
        const auto dx = x - black_hole.position.x;
        const auto dy = base_grid_height - black_hole.position.y;
        const auto dz = z - black_hole.position.z;
        const auto softening = std::max(black_hole.event_horizon, 0.1);
        const auto amplitude = 0.45 * std::max(black_hole.mass, 0.0) * softening;
        const auto depth = amplitude / std::sqrt(dx * dx + dy * dy + dz * dz + softening * softening);
        return static_cast<float>(base_grid_height - std::min(depth, 4.0));
    };
    if (show_grid) {
        // Subdivide every configured grid line so it follows the gravity well smoothly.
        const auto lines = grid_plane.grid_lines();
        const auto subdivisions = std::clamp(
            static_cast<int>(std::ceil(grid_plane.size / 0.125)), 2, 256);
        std::vector<Vertex> grid;
        grid.reserve((lines.size() / 2) * static_cast<std::size_t>(subdivisions) * 2);
        for (std::size_t line = 0; line + 1 < lines.size(); line += 2) {
            const auto start = lines[line];
            const auto end = lines[line + 1];
            for (int step = 0; step < subdivisions; ++step) {
                const auto p0 = glm::mix(start, end,
                                         static_cast<double>(step) / subdivisions);
                const auto p1 = glm::mix(start, end,
                                         static_cast<double>(step + 1) / subdivisions);
                grid.push_back({static_cast<float>(p0.x), bent_height(p0.x, p0.z),
                                static_cast<float>(p0.z)});
                grid.push_back({static_cast<float>(p1.x), bent_height(p1.x, p1.z),
                                static_cast<float>(p1.z)});
            }
        }
        GLfloat line_width_range[2]{1.0F, 1.0F};
        glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range);
        const auto line_width = std::clamp(
            1.0F + static_cast<float>(grid_plane.line_thickness) * 20.0F,
            line_width_range[0], line_width_range[1]);
        glLineWidth(line_width);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        upload_and_draw(grid, GL_LINES,
                        {0.10F, 0.23F, 0.34F,
                         static_cast<float>(std::clamp(grid_plane.line_opacity, 0.0, 1.0))});
        glDisable(GL_BLEND);
        glLineWidth(1.0F);
    }

    // Draw the scene normally. The post-process bends its camera rays afterward.
    glUniform1i(planet_material_uniform_, GL_TRUE);
    for (const auto& planet : planets) {
        const auto planet_transform =
            glm::translate(glm::mat4{1.0F}, glm::vec3{static_cast<float>(planet.position.x),
                                                      static_cast<float>(planet.position.y),
                                                      static_cast<float>(planet.position.z)}) *
            glm::rotate(glm::mat4{1.0F}, static_cast<float>(planet.rotation), glm::vec3{0.0F, 1.0F, 0.0F}) *
            glm::scale(glm::mat4{1.0F}, glm::vec3{static_cast<float>(planet.radius)});
        glUniformMatrix4fv(model_matrix_uniform_, 1, GL_FALSE, glm::value_ptr(planet_transform));
        glUniform3fv(planet_color_uniform_, 1, glm::value_ptr(planet.color));
        upload_and_draw(unitSphereMesh(), GL_TRIANGLES, {1.0F, 1.0F, 1.0F, 1.0F}, false, true);
    }
    glUniform1i(planet_material_uniform_, GL_FALSE);

    // Particles own their shader and empty VAO; the scene program stays focused
    // on geometry and avoids carrying particle uniforms through every draw.
    particle_renderer_.draw(particles, black_hole, static_cast<float>(grid_plane.size),
                            view_projection, camera_position);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    const auto shadow_radius = lens_center_clip.w > 0.0F
        ? horizon * 1.30F / std::max(lens_distance, 0.01F) * projection[1][1]
        : 0.0F;
    const auto lens_depth = lens_center_clip.w > 0.0F
        ? lens_center_clip.z / lens_center_clip.w * 0.5F + 0.5F : 1.0F;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.012F, 0.018F, 0.035F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(static_cast<GLuint>(lensing_program_));
    glBindVertexArray(static_cast<GLuint>(lensing_vertex_array_));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(scene_texture_));
    glUniform1i(scene_texture_uniform_, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(depth_texture_));
    glUniform1i(scene_depth_uniform_, 1);
    glUniform2fv(lensing_center_ndc_uniform_, 1, glm::value_ptr(lens_center_ndc));
    glUniform1f(lensing_aspect_uniform_, aspect);
    glUniform1f(einstein_radius_uniform_, einstein_radius);
    glUniform1f(shadow_radius_uniform_, shadow_radius);
    glUniform1f(lens_depth_uniform_, lens_depth);
    glUniform3f(background_color_uniform_, 0.012F, 0.018F, 0.035F);
    glUniform2f(viewport_size_uniform_, static_cast<float>(width), static_cast<float>(height));
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawAtomic(const physics::atom& atom, const Camera& camera,
                          int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.006F, 0.009F, 0.018F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    const auto aspect = static_cast<float>(width) / static_cast<float>(height);
    const auto view_projection = camera.projectionMatrix(aspect) * camera.viewMatrix();
    atom_renderer_.draw(atom, view_projection);
}

void Renderer::drawSelection(int width, int height) const noexcept {
    if (!initialized_ || width <= 0 || height <= 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.006F, 0.009F, 0.018F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::release() noexcept {
    const auto buffer = static_cast<GLuint>(vertex_buffer_);
    const auto array = static_cast<GLuint>(vertex_array_);
    const auto lensing_array = static_cast<GLuint>(lensing_vertex_array_);
    const auto framebuffer = static_cast<GLuint>(framebuffer_);
    const auto scene_texture = static_cast<GLuint>(scene_texture_);
    const auto depth_texture = static_cast<GLuint>(depth_texture_);
    if (buffer != 0) glDeleteBuffers(1, &buffer);
    if (array != 0) glDeleteVertexArrays(1, &array);
    if (lensing_array != 0) glDeleteVertexArrays(1, &lensing_array);
    if (framebuffer != 0) glDeleteFramebuffers(1, &framebuffer);
    if (scene_texture != 0) glDeleteTextures(1, &scene_texture);
    if (depth_texture != 0) glDeleteTextures(1, &depth_texture);
    if (program_ != 0) glDeleteProgram(static_cast<GLuint>(program_));
    if (lensing_program_ != 0) glDeleteProgram(static_cast<GLuint>(lensing_program_));
    vertex_buffer_ = 0;
    vertex_array_ = 0;
    lensing_vertex_array_ = 0;
    framebuffer_ = 0;
    scene_texture_ = 0;
    depth_texture_ = 0;
    render_target_width_ = 0;
    render_target_height_ = 0;
    program_ = 0;
    lensing_program_ = 0;
    initialized_ = false;
    particle_renderer_.release();
    atom_renderer_.release();
}

} // namespace opengl_sim::render
