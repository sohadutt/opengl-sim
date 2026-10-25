#include "opengl_sim/render/atom_renderer.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace opengl_sim::render {
namespace {

constexpr int probability_samples = 1024;
constexpr int max_orbitals = 19;
constexpr int excited_orbital_row = max_orbitals;
constexpr int ground_density_row = max_orbitals + 1;
constexpr int excited_density_row = max_orbitals + 2;
constexpr int texture_rows = max_orbitals + 3;
constexpr int electron_cloud_samples = 120'000;
constexpr int photon_wave_samples = 256;

constexpr char atom_vertex_shader[] = R"glsl(
#version 410 core
uniform mat4 view_projection;
uniform sampler2D probability_texture;
uniform int orbital_count;
uniform float cumulative_occupancy[19];
uniform float max_radius_bohr;
uniform bool nucleus_pass;
uniform bool photon_pass;
uniform int proton_count;
uniform int neutron_count;
uniform int electron_count;
uniform int excited_orbital_row;
uniform int ground_density_row;
uniform int excited_density_row;
uniform float excitation_probability;
uniform float photon_center_x;
uniform float photon_wave_number;
uniform float photon_amplitude;
uniform float electron_size;
uniform int electron_pattern;
out vec3 point_color;
out float point_alpha;

float random_value(float id, float seed) {
    return fract(id * seed + 0.17320508);
}

vec3 heat_color(float value) {
    vec3 cool_blue = vec3(0.06, 0.20, 0.72);
    vec3 cyan = vec3(0.04, 0.68, 0.92);
    vec3 yellow = vec3(0.92, 0.88, 0.18);
    vec3 orange = vec3(1.0, 0.48, 0.08);
    vec3 hot_red = vec3(0.92, 0.035, 0.025);
    if (value < 0.25) return mix(cool_blue, cyan, value * 4.0);
    if (value < 0.5) return mix(cyan, yellow, (value - 0.25) * 4.0);
    if (value < 0.75) return mix(yellow, orange, (value - 0.5) * 4.0);
    return mix(orange, hot_red, (value - 0.75) * 4.0);
}

void main() {
    float id = float(gl_VertexID);
    if (photon_pass) {
        float local_x = (id / 255.0 - 0.5) * 6.4;
        float envelope = exp(-0.5 * local_x * local_x / (1.6 * 1.6));
        float phase = photon_wave_number * local_x;
        float wave = sin(phase);
        vec3 position = vec3(photon_center_x + local_x,
                             0.20 * envelope * wave, 0.0);
        gl_Position = view_projection * vec4(position, 1.0);
        gl_PointSize = 3.0;
        point_color = vec3(0.20, 0.76, 1.0);
        point_alpha = photon_amplitude * photon_amplitude * envelope *
                      (0.3 + 0.7 * abs(wave));
        return;
    }

    if (nucleus_pass) {
        int total_nucleons = proton_count + neutron_count;
        float radial = 0.055 * pow(random_value(id, 0.618033989), 1.0 / 3.0);
        float cos_theta = 2.0 * random_value(id, 0.754877666) - 1.0;
        float phi = 6.283185307 * random_value(id, 0.569840296);
        float ring = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
        vec3 position = radial * vec3(ring * cos(phi), cos_theta, ring * sin(phi));
        gl_Position = view_projection * vec4(position, 1.0);
        gl_PointSize = 6.0;
        point_color = int(id) < proton_count ? vec3(1.0, 0.18, 0.08)
                                             : vec3(0.76, 0.82, 0.91);
        point_alpha = total_nucleons > 0 ? 1.0 : 0.0;
        return;
    }

    float orbital_choice = random_value(id, 0.618033989);
    float excited_weight = clamp(excitation_probability, 0.0, 1.0) /
                           max(float(electron_count), 1.0);
    bool excited_sample = orbital_choice < excited_weight;
    int orbital_index = max(orbital_count - 1, 0);
    if (excited_sample) {
        orbital_index = excited_orbital_row;
    } else {
        float ground_choice = (orbital_choice - excited_weight) /
                              max(1.0 - excited_weight, 1e-6);
        for (int index = 0; index < orbital_count; ++index) {
            if (ground_choice <= cumulative_occupancy[index]) {
                orbital_index = index;
                break;
            }
        }
    }

    float radial_choice = random_value(id, 0.414213562);
    int lower = 0;
    int upper = 1023;
    for (int step = 0; step < 10; ++step) {
        int middle = (lower + upper) / 2;
        float cdf = texelFetch(probability_texture, ivec2(middle, orbital_index), 0).r;
        if (cdf < radial_choice) lower = middle;
        else upper = middle;
    }

    float radial_fraction = float(upper) / 1023.0;
    float angular_random = 2.0 * random_value(id, 0.754877666) - 1.0;
    float phi = 6.283185307 * random_value(id, 0.569840296);
    float ring = sqrt(max(1.0 - angular_random * angular_random, 0.0));
    vec3 direction = vec3(ring * cos(phi), angular_random, ring * sin(phi));
    if (electron_pattern == 1 || electron_pattern == 2) {
        // Rejection sampling produces recognizable p and d angular lobes
        // while retaining the radial distribution of the selected atom.
        for (int attempt = 0; attempt < 32; ++attempt) {
            float z = 2.0 * random_value(id + float(attempt) * 97.0,
                                         0.754877666) - 1.0;
            float angle = 6.283185307 * random_value(
                id + float(attempt) * 193.0, 0.569840296);
            float r = sqrt(max(1.0 - z * z, 0.0));
            vec3 candidate = vec3(r * cos(angle), z, r * sin(angle));
            float angular_density = electron_pattern == 1
                ? z * z
                : 0.25 * (3.0 * z * z - 1.0) * (3.0 * z * z - 1.0);
            float acceptance = random_value(id + float(attempt) * 389.0,
                                            0.618033989);
            if (acceptance <= angular_density) {
                direction = candidate;
                break;
            }
        }
    } else if (excited_sample) {
        float z = sign(angular_random) * pow(abs(angular_random), 1.0 / 3.0);
        float excited_ring = sqrt(max(1.0 - z * z, 0.0));
        direction = vec3(excited_ring * cos(phi), z, excited_ring * sin(phi));
    }
    float cos_theta = direction.y;
    float cloud_extent = clamp(0.95 + 0.10 * log(1.0 + max_radius_bohr), 1.0, 1.8);
    vec3 position = direction * (radial_fraction * cloud_extent);

    int selected_density_row = excited_sample ? excited_density_row : ground_density_row;
    float density = texelFetch(probability_texture,
        ivec2(upper, selected_density_row), 0).r;
    if (excited_sample) density *= 3.0 * cos_theta * cos_theta;
    float heat = clamp(log(1.0 + 100.0 * density) / log(101.0), 0.0, 1.0);
    gl_Position = view_projection * vec4(position, 1.0);
    gl_PointSize = electron_size * (excited_sample ? 1.35 : 1.0);
    point_color = heat_color(heat);
    point_alpha = density > 0.0005
        ? (0.08 + 0.42 * heat) * (excited_sample ? 1.15 : 1.0) : 0.0;
}
)glsl";

constexpr char atom_fragment_shader[] = R"glsl(
#version 410 core
in vec3 point_color;
in float point_alpha;
out vec4 output_color;
void main() {
    float radius = length(gl_PointCoord - vec2(0.5)) * 2.0;
    if (radius > 1.0 || point_alpha <= 0.001) discard;
    float soft_edge = exp(-radius * radius * 2.0);
    output_color = vec4(point_color, point_alpha * soft_edge);
}
)glsl";

double gridRadius(double maximum_radius, int sample) {
    const auto fraction = static_cast<double>(sample) /
                          static_cast<double>(probability_samples - 1);
    return maximum_radius * fraction * fraction;
}

} // namespace

AtomRenderer::AtomRenderer(core::Logger& logger) noexcept : logger_(logger) {}
AtomRenderer::~AtomRenderer() { release(); }

std::uint32_t AtomRenderer::compileShader(std::uint32_t type, const char* source) {
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
        logger_.error("Atomic shader compilation failed: " + message);
        glDeleteShader(shader);
        return 0;
    }
    return static_cast<std::uint32_t>(shader);
}

bool AtomRenderer::initialize() {
    const auto vertex_shader = compileShader(GL_VERTEX_SHADER, atom_vertex_shader);
    const auto fragment_shader = compileShader(GL_FRAGMENT_SHADER, atom_fragment_shader);
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
        logger_.critical("Atomic shader program linking failed: " + message);
        release();
        return false;
    }

    view_projection_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "view_projection");
    probability_texture_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "probability_texture");
    orbital_count_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "orbital_count");
    cumulative_occupancy_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "cumulative_occupancy[0]");
    max_radius_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "max_radius_bohr");
    nucleus_pass_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "nucleus_pass");
    photon_pass_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "photon_pass");
    proton_count_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "proton_count");
    neutron_count_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "neutron_count");
    electron_count_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "electron_count");
    excited_orbital_row_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "excited_orbital_row");
    ground_density_row_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "ground_density_row");
    excited_density_row_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "excited_density_row");
    excitation_probability_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "excitation_probability");
    photon_center_x_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "photon_center_x");
    photon_wave_number_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "photon_wave_number");
    photon_amplitude_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "photon_amplitude");
    electron_size_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "electron_size");
    electron_pattern_uniform_ = glGetUniformLocation(static_cast<GLuint>(program_), "electron_pattern");

    GLuint vertex_array = 0;
    GLuint texture = 0;
    glGenVertexArrays(1, &vertex_array);
    glGenTextures(1, &texture);
    vertex_array_ = vertex_array;
    probability_texture_ = texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    initialized_ = true;
    return true;
}

void AtomRenderer::updateProbabilityTexture(const physics::atom& atom_model) {
    constexpr auto samples = static_cast<std::size_t>(probability_samples);
    std::vector<float> texture_data(samples * static_cast<std::size_t>(texture_rows), 0.0F);
    cached_orbitals_ = atom_model.ground_state_orbitals();
    cached_max_radius_bohr_ = atom_model.maximum_orbital_radius_bohr();
    const auto& orbitals = cached_orbitals_;
    const auto max_radius = cached_max_radius_bohr_;
    const auto effective_charge = orbitals.empty()
                                      ? static_cast<double>(std::max(atom_model.proton_count, 1))
                                      : orbitals.front().effective_nuclear_charge;
    const physics::atomic_orbital excited_orbital{2, 1, 1, effective_charge};
    const auto fill_radial_cdf = [&](std::size_t row, const physics::atomic_orbital& orbital) {
        auto integrated_probability = 0.0;
        auto previous_radius = gridRadius(max_radius, 0);
        auto previous_probability = atom_model.orbital_radial_probability(orbital, previous_radius);
        for (int sample = 1; sample < probability_samples; ++sample) {
            const auto radius = gridRadius(max_radius, sample);
            const auto probability = atom_model.orbital_radial_probability(orbital, radius);
            integrated_probability += 0.5 * (previous_probability + probability) *
                                      (radius - previous_radius);
            texture_data[row * samples + static_cast<std::size_t>(sample)] =
                static_cast<float>(integrated_probability);
            previous_radius = radius;
            previous_probability = probability;
        }
        const auto normalization = std::max(integrated_probability, 1e-30);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            texture_data[row * samples + sample] /=
                static_cast<float>(normalization);
        }
        texture_data[row * samples + samples - 1] = 1.0F;
    };
    for (std::size_t orbital_index = 0; orbital_index < orbitals.size(); ++orbital_index) {
        fill_radial_cdf(orbital_index, orbitals[orbital_index]);
    }
    fill_radial_cdf(static_cast<std::size_t>(excited_orbital_row), excited_orbital);

    auto maximum_ground_density = 0.0;
    auto maximum_excited_density = 0.0;
    for (int sample = 0; sample < probability_samples; ++sample) {
        const auto radius = gridRadius(max_radius, sample);
        maximum_ground_density = std::max(
            maximum_ground_density,
            atom_model.electron_density_per_bohr_cubed(radius, orbitals));
        maximum_excited_density = std::max(
            maximum_excited_density,
            atom_model.orbital_radial_probability(excited_orbital, radius));
    }
    if (maximum_ground_density > 0.0) {
        const auto row_offset = static_cast<std::size_t>(ground_density_row) * samples;
        for (int sample = 0; sample < probability_samples; ++sample) {
            const auto density = atom_model.electron_density_per_bohr_cubed(
                gridRadius(max_radius, sample), orbitals);
            texture_data[row_offset + static_cast<std::size_t>(sample)] =
                static_cast<float>(density / maximum_ground_density);
        }
    }
    if (maximum_excited_density > 0.0) {
        const auto row_offset = static_cast<std::size_t>(excited_density_row) * samples;
        for (int sample = 0; sample < probability_samples; ++sample) {
            const auto density = atom_model.orbital_radial_probability(
                excited_orbital, gridRadius(max_radius, sample));
            texture_data[row_offset + static_cast<std::size_t>(sample)] =
                static_cast<float>(density / maximum_excited_density);
        }
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(probability_texture_));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, probability_samples, texture_rows, 0,
                 GL_RED, GL_FLOAT, texture_data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    last_proton_count_ = atom_model.proton_count;
    last_neutron_count_ = atom_model.neutron_count;
    last_electron_count_ = atom_model.electron_count;
}

void AtomRenderer::draw(const physics::atom& atom_model,
                        const glm::mat4& view_projection) {
    if (!initialized_) return;
    if (last_proton_count_ != atom_model.proton_count ||
        last_neutron_count_ != atom_model.neutron_count ||
        last_electron_count_ != atom_model.electron_count) {
        updateProbabilityTexture(atom_model);
    }

    const auto& orbitals = cached_orbitals_;
    std::array<float, max_orbitals> cumulative_occupancy{};
    auto total_occupancy = 0;
    for (const auto& orbital : orbitals) total_occupancy += orbital.electron_occupancy;
    auto cumulative = 0;
    for (std::size_t index = 0; index < orbitals.size(); ++index) {
        cumulative += orbitals[index].electron_occupancy;
        cumulative_occupancy[index] = total_occupancy > 0
            ? static_cast<float>(cumulative) / static_cast<float>(total_occupancy) : 0.0F;
    }

    glUseProgram(static_cast<GLuint>(program_));
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, &view_projection[0][0]);
    glUniform1i(probability_texture_uniform_, 0);
    glUniform1i(orbital_count_uniform_, static_cast<GLint>(orbitals.size()));
    glUniform1fv(cumulative_occupancy_uniform_, max_orbitals, cumulative_occupancy.data());
    glUniform1f(max_radius_uniform_, static_cast<float>(cached_max_radius_bohr_));
    glUniform1i(proton_count_uniform_, std::clamp(atom_model.proton_count, 0, 118));
    glUniform1i(neutron_count_uniform_, std::clamp(atom_model.neutron_count, 0, 300));
    glUniform1i(electron_count_uniform_, std::clamp(atom_model.electron_count, 0, 118));
    glUniform1i(electron_pattern_uniform_, static_cast<GLint>(atom_model.visual_pattern));
    glUniform1i(excited_orbital_row_uniform_, excited_orbital_row);
    glUniform1i(ground_density_row_uniform_, ground_density_row);
    glUniform1i(excited_density_row_uniform_, excited_density_row);
    glUniform1f(excitation_probability_uniform_,
                static_cast<float>(atom_model.excitation_probability()));
    glUniform1f(electron_size_uniform_,
                static_cast<float>(std::clamp(atom_model.electron_size, 1.0, 12.0)));
    glUniform1i(photon_pass_uniform_, GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(probability_texture_));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    if (!orbitals.empty()) {
        glUniform1i(nucleus_pass_uniform_, GL_FALSE);
        glDrawArrays(GL_POINTS, 0, electron_cloud_samples);
    }
    const auto nucleons = std::clamp(atom_model.proton_count, 0, 118) +
                          std::clamp(atom_model.neutron_count, 0, 300);
    if (nucleons > 0) {
        glUniform1i(nucleus_pass_uniform_, GL_TRUE);
        glDrawArrays(GL_POINTS, 0, nucleons);
    }
    if (atom_model.photon_visible()) {
        glUniform1i(nucleus_pass_uniform_, GL_FALSE);
        glUniform1i(photon_pass_uniform_, GL_TRUE);
        glUniform1f(photon_center_x_uniform_, static_cast<float>(atom_model.photon_center_x()));
        glUniform1f(photon_wave_number_uniform_,
                    static_cast<float>(atom_model.photon_visual_wave_number()));
        glUniform1f(photon_amplitude_uniform_,
                    static_cast<float>(std::sqrt(atom_model.photon_survival_probability())));
        glDrawArrays(GL_POINTS, 0, photon_wave_samples);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void AtomRenderer::release() noexcept {
    const auto array = static_cast<GLuint>(vertex_array_);
    const auto texture = static_cast<GLuint>(probability_texture_);
    if (array != 0) glDeleteVertexArrays(1, &array);
    if (texture != 0) glDeleteTextures(1, &texture);
    if (program_ != 0) glDeleteProgram(static_cast<GLuint>(program_));
    vertex_array_ = 0;
    probability_texture_ = 0;
    program_ = 0;
    initialized_ = false;
}

} // namespace opengl_sim::render
