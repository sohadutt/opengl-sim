#include "opengl_sim/render/particle_renderer.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>

namespace opengl_sim::render {
namespace {

constexpr char particle_vertex_shader[] = R"glsl(
#version 410 core
uniform mat4 view_projection;
uniform float particle_size;
uniform int trail_samples;
uniform float particle_time;
uniform float particle_speed;
uniform float particle_extent;
uniform float particle_horizon;
uniform float particle_mass;
uniform float particle_spin;
uniform vec3 black_hole_position;
uniform vec3 camera_position;
out vec3 particle_color;
out float particle_alpha;

float hash_id(float value) {
    // Independent low-cost sequences avoid a trigonometric hash per vertex.
    return fract(value * 0.618033989 + 0.13787);
}

vec3 heat_color(float value) {
    vec3 dark_red = vec3(0.38, 0.025, 0.008);
    vec3 orange = vec3(1.0, 0.19, 0.025);
    vec3 amber = vec3(1.0, 0.42, 0.08);
    vec3 gold = vec3(1.0, 0.67, 0.20);
    vec3 hot_white = vec3(1.0, 0.94, 0.78);
    if (value < 0.25) return mix(dark_red, orange, value * 4.0);
    if (value < 0.5) return mix(orange, amber, (value - 0.25) * 4.0);
    if (value < 0.75) return mix(amber, gold, (value - 0.5) * 4.0);
    return mix(gold, hot_white, (value - 0.75) * 4.0);
}

void main() {
    float sample_count = float(max(trail_samples, 1));
    float vertex_id = float(gl_VertexID);
    float sample_index = mod(vertex_id, sample_count);
    float particle_id = floor(vertex_id / sample_count);
    float trail_progress = sample_count > 1.0
        ? sample_index / (sample_count - 1.0) : 0.0;

    float radial_random = hash_id(particle_id);
    float azimuth_random = fract(particle_id * 0.754877666 + 0.321);
    float height_random = fract(particle_id * 0.569840296 + 0.713);
    // Bias a little more material toward the hot inner disk while retaining a
    // broad outer distribution. All samples for one particle share this orbit.
    float radius_fraction = pow(radial_random, 1.35);
    float radius = particle_horizon * 1.5 + radius_fraction * particle_extent;
    float height = (height_random - 0.5) * min(radius * 0.045, particle_horizon * 0.28);
    float ring_radius = sqrt(max(radius * radius - height * height, 0.0));
    float radius_ratio = max(radius / max(particle_horizon, 0.001), 1.0);

    // Kepler-like falloff: inner orbits move faster than distant particles.
    float mass_scale = sqrt(max(particle_mass, 0.01));
    float angular_speed = clamp(particle_speed * mass_scale *
                                 pow(radius_ratio, -1.5), 0.008, 20.0);
    float inclination = height / max(radius, 0.001);
    angular_speed *= 1.0 + clamp(particle_spin, -1.0, 1.0) * inclination * 0.22;
    float orbital_phase = azimuth_random * 6.283185307;
    float sample_time = particle_time - trail_progress * 0.72;
    float precession = particle_spin * sample_time * 0.025 / radius_ratio;
    float angle = orbital_phase + sample_time * angular_speed + precession;

    vec3 offset = vec3(cos(angle) * ring_radius, height, sin(angle) * ring_radius);
    vec3 particle_position = black_hole_position + offset;
    float heat_radius = particle_horizon * (1.0 + clamp(particle_mass, 0.0, 10.0) * 0.1);
    float heat = exp(-max(radius - particle_horizon * 1.5, 0.0) /
                     max(2.5 * heat_radius, 0.001));

    // Mild relativistic beaming makes the approaching side brighter without
    // replacing the camera-space gravitational lensing pass.
    vec3 tangent = normalize(vec3(-sin(angle), 0.0, cos(angle)));
    float view_alignment = dot(tangent, normalize(camera_position - particle_position));
    float beaming = clamp(1.0 + 0.32 * view_alignment * angular_speed, 0.65, 1.45);

    gl_Position = view_projection * vec4(particle_position, 1.0);
    gl_PointSize = particle_size * (0.78 + heat * 0.72);
    particle_color = heat_color(heat) * beaming;
    particle_alpha = exp(-trail_progress * 2.0);
}
)glsl";

constexpr char particle_fragment_shader[] = R"glsl(
#version 410 core
in vec3 particle_color;
in float particle_alpha;
out vec4 output_color;
void main() {
    float radius = length(gl_PointCoord - vec2(0.5)) * 2.0;
    if (radius > 1.0) discard;
    float soft_glow = exp(-radius * radius * 3.0);
    output_color = vec4(particle_color, soft_glow * 0.30 * particle_alpha);
}
)glsl";

} // namespace

ParticleRenderer::ParticleRenderer(core::Logger& logger) noexcept : logger_(logger) {}
ParticleRenderer::~ParticleRenderer() { release(); }

std::uint32_t ParticleRenderer::compileShader(std::uint32_t type, const char* source) {
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
        logger_.error("Particle shader compilation failed: " + message);
        glDeleteShader(shader);
        return 0;
    }
    return static_cast<std::uint32_t>(shader);
}

bool ParticleRenderer::initialize() {
    const auto vertex_shader = compileShader(GL_VERTEX_SHADER, particle_vertex_shader);
    const auto fragment_shader = compileShader(GL_FRAGMENT_SHADER, particle_fragment_shader);
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
        logger_.critical("Particle shader program linking failed: " + message);
        release();
        return false;
    }

    view_projection_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "view_projection");
    particle_size_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_size");
    trail_samples_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "trail_samples");
    particle_time_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_time");
    particle_speed_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_speed");
    particle_extent_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_extent");
    particle_horizon_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_horizon");
    particle_mass_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_mass");
    particle_spin_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "particle_spin");
    black_hole_position_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "black_hole_position");
    camera_position_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "camera_position");

    GLuint vertex_array = 0;
    glGenVertexArrays(1, &vertex_array);
    vertex_array_ = vertex_array;
    initialized_ = true;
    return true;
}

void ParticleRenderer::draw(const physics::light_particle_field& particles,
                            const physics::black_hole& black_hole,
                            float scene_extent,
                            const glm::mat4& view_projection,
                            const glm::vec3& camera_position) const noexcept {
    if (!initialized_) return;
    const auto particle_count = std::clamp(particles.count, 1, 1'000'000);
    const auto trail_samples = particles.trails_enabled ? 4 : 1;
    const auto black_hole_position = glm::vec3{black_hole.position};

    glUseProgram(static_cast<GLuint>(program_));
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, &view_projection[0][0]);
    glUniform1f(particle_size_uniform_, static_cast<float>(std::clamp(particles.size, 1.0, 12.0)));
    glUniform1i(trail_samples_uniform_, trail_samples);
    glUniform1f(particle_time_uniform_, static_cast<float>(particles.elapsed_time));
    glUniform1f(particle_speed_uniform_, static_cast<float>(std::clamp(particles.speed, 0.01, 20.0)));
    glUniform1f(particle_extent_uniform_, std::max(scene_extent, 1.0F));
    glUniform1f(particle_horizon_uniform_, static_cast<float>(std::max(black_hole.event_horizon, 0.01)));
    glUniform1f(particle_mass_uniform_, static_cast<float>(std::max(black_hole.mass, 0.01)));
    glUniform1f(particle_spin_uniform_, static_cast<float>(std::clamp(black_hole.spin, -1.0, 1.0)));
    glUniform3fv(black_hole_position_uniform_, 1, &black_hole_position.x);
    glUniform3fv(camera_position_uniform_, 1, &camera_position.x);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS, 0, particle_count * trail_samples);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleRenderer::release() noexcept {
    const auto vertex_array = static_cast<GLuint>(vertex_array_);
    if (vertex_array != 0) glDeleteVertexArrays(1, &vertex_array);
    if (program_ != 0) glDeleteProgram(static_cast<GLuint>(program_));
    vertex_array_ = 0;
    program_ = 0;
    initialized_ = false;
}

} // namespace opengl_sim::render
