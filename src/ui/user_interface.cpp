#include "opengl_sim/ui/user_interface.hpp"
#include "opengl_sim/core/heatmap.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace opengl_sim::ui {
namespace {

constexpr char vertex_shader_source[] = R"glsl(
#version 410 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec4 vertex_color;
uniform vec2 screen_size;
out vec4 color;
void main() {
    vec2 clip = vec2(2.0 * position.x / screen_size.x - 1.0,
                     1.0 - 2.0 * position.y / screen_size.y);
    gl_Position = vec4(clip, 0.0, 1.0);
    color = vertex_color;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(
#version 410 core
in vec4 color;
out vec4 output_color;
void main() {
    output_color = color;
}
)glsl";

struct Vertex {
    float x;
    float y;
    float red;
    float green;
    float blue;
    float alpha;
};

using Glyph = std::array<unsigned char, 5>;

Glyph glyphFor(char character) {
    switch (character) {
    case 'A': return {2, 5, 7, 5, 5};
    case 'B': return {6, 5, 6, 5, 6};
    case 'C': return {3, 4, 4, 4, 3};
    case 'D': return {6, 5, 5, 5, 6};
    case 'E': return {7, 4, 6, 4, 7};
    case 'F': return {7, 4, 6, 4, 4};
    case 'G': return {3, 4, 5, 5, 3};
    case 'H': return {5, 5, 7, 5, 5};
    case 'I': return {7, 2, 2, 2, 7};
    case 'J': return {1, 1, 1, 5, 2};
    case 'K': return {5, 5, 6, 5, 5};
    case 'L': return {4, 4, 4, 4, 7};
    case 'M': return {5, 7, 7, 5, 5};
    case 'N': return {5, 7, 7, 7, 5};
    case 'O': return {2, 5, 5, 5, 2};
    case 'P': return {6, 5, 6, 4, 4};
    case 'Q': return {2, 5, 5, 7, 3};
    case 'R': return {6, 5, 6, 5, 5};
    case 'S': return {3, 4, 2, 1, 6};
    case 'T': return {7, 2, 2, 2, 2};
    case 'U': return {5, 5, 5, 5, 7};
    case 'V': return {5, 5, 5, 5, 2};
    case 'W': return {5, 5, 7, 7, 5};
    case 'X': return {5, 5, 2, 5, 5};
    case 'Y': return {5, 5, 2, 2, 2};
    case 'Z': return {7, 1, 2, 4, 7};
    case '0': return {7, 5, 5, 5, 7};
    case '1': return {2, 6, 2, 2, 7};
    case '2': return {6, 1, 2, 4, 7};
    case '3': return {6, 1, 3, 1, 6};
    case '4': return {5, 5, 7, 1, 1};
    case '5': return {7, 4, 6, 1, 6};
    case '6': return {3, 4, 6, 5, 2};
    case '7': return {7, 1, 2, 2, 2};
    case '8': return {2, 5, 2, 5, 2};
    case '9': return {2, 5, 3, 1, 6};
    case '.': return {0, 0, 0, 0, 2};
    case '-': return {0, 0, 7, 0, 0};
    case '+': return {0, 2, 7, 2, 0};
    default: return {0, 0, 0, 0, 0};
    }
}

void addRectangle(std::vector<Vertex>& vertices, float x, float y, float width,
                  float height, const std::array<float, 4>& color) {
    const auto add = [&vertices, &color](float px, float py) {
        vertices.push_back({px, py, color[0], color[1], color[2], color[3]});
    };
    add(x, y);
    add(x + width, y);
    add(x + width, y + height);
    add(x, y);
    add(x + width, y + height);
    add(x, y + height);
}

void addText(std::vector<Vertex>& vertices, std::string_view text, float x, float y,
             float scale, const std::array<float, 4>& color) {
    auto cursor_x = x;
    for (const auto raw_character : text) {
        const auto character = raw_character >= 'a' && raw_character <= 'z'
                                   ? static_cast<char>(raw_character - 'a' + 'A')
                                   : raw_character;
        const auto glyph = glyphFor(character);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((glyph[row] & (1U << (2 - column))) != 0) {
                    addRectangle(vertices, cursor_x + static_cast<float>(column) * scale,
                                 y + static_cast<float>(row) * scale, scale, scale, color);
                }
            }
        }
        cursor_x += 4.0F * scale;
    }
}

std::string formatValue(double value, bool integral) {
    if (integral) return std::to_string(static_cast<long long>(std::llround(value)));
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << value;
    auto text = output.str();
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

GLuint compileShader(GLenum type, const char* source, core::Logger& logger) {
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, message.data());
        logger.error("UI shader compilation failed: " + message);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

UserInterface::UserInterface(core::Logger& logger) noexcept : logger_(logger) {}
UserInterface::~UserInterface() { release(); }

void UserInterface::addSliderControl(std::string label, std::function<double()> get_value,
                                     std::function<void(double)> set_value, double minimum,
                                     double maximum, bool integral) {
    if (maximum <= minimum) {
        logger_.warning("Ignoring UI slider '" + label + "' with an invalid range.");
        return;
    }
    sliders_.push_back({std::move(label), std::move(get_value), std::move(set_value),
                        minimum, maximum, integral});
}

void UserInterface::addSlider(std::string label, std::function<double()> get_value,
                              std::function<void(double)> set_value, double minimum,
                              double maximum) {
    addSliderControl(std::move(label), std::move(get_value), std::move(set_value),
                     minimum, maximum, false);
}

void UserInterface::addButton(std::string label, std::function<void()> action) {
    buttons_.push_back({std::move(label), std::move(action)});
}

void UserInterface::addToggle(std::string label, bool& value) {
    toggles_.push_back({std::move(label), &value});
}

void UserInterface::addInfo(std::function<std::string()> text) {
    info_labels_.push_back({std::move(text)});
}

void UserInterface::clearControls() {
    sliders_.clear();
    buttons_.clear();
    toggles_.clear();
    info_labels_.clear();
    heatmap_legend_.clear();
    dragging_slider_ = -1;
}

void UserInterface::setPanelTitle(std::string title) {
    panel_title_ = std::move(title);
}

void UserInterface::setHeatmapLegend(std::string label) {
    heatmap_legend_ = std::move(label);
}

bool UserInterface::initialize() {
#if !defined(__APPLE__)
    glewExperimental = GL_TRUE;
    const auto status = glewInit();
    if (status != GLEW_OK) {
        logger_.critical("Could not initialize GLEW for the UI renderer.");
        return false;
    }
    static_cast<void>(glGetError());
#endif

    const auto vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_shader_source, logger_);
    const auto fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_shader_source, logger_);
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        return false;
    }

    const auto program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(program, length, nullptr, message.data());
        logger_.critical("UI shader program link failed: " + message);
        glDeleteProgram(program);
        return false;
    }

    program_ = program;
    resolution_uniform_ = glGetUniformLocation(program, "screen_size");
    GLuint array = 0;
    GLuint buffer = 0;
    glGenVertexArrays(1, &array);
    glGenBuffers(1, &buffer);
    vertex_array_ = array;
    vertex_buffer_ = buffer;
    glBindVertexArray(array);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, red)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    initialized_ = true;
    logger_.info("UI initialized.");
    return true;
}

void UserInterface::setPointerPosition(double x, double y) noexcept {
    pointer_x_ = x;
    pointer_y_ = y;
    updateDraggedSlider();
}

bool UserInterface::pointerOverPanel() const noexcept {
    constexpr double panel_left = 16.0;
    constexpr double panel_top = 16.0;
    constexpr double panel_width = 300.0;
    constexpr double row_height = 52.0;
    constexpr double toggle_height = 32.0;
    constexpr double info_height = 26.0;
    const auto panel_height = 54.0 + static_cast<double>(sliders_.size()) * row_height +
                              static_cast<double>(toggles_.size()) * toggle_height +
                              static_cast<double>(info_labels_.size()) * info_height +
                              static_cast<double>(buttons_.size()) * 40.0;
    return pointer_x_ >= panel_left && pointer_x_ <= panel_left + panel_width &&
           pointer_y_ >= panel_top && pointer_y_ <= panel_top + panel_height;
}

void UserInterface::onPrimaryButton(bool pressed) {
    if (!pressed) {
        dragging_slider_ = -1;
        return;
    }

    const auto panel_x = 16.0;
    const auto panel_y = 16.0;
    constexpr double panel_width = 300.0;
    constexpr double padding = 12.0;
    constexpr double row_height = 52.0;
    constexpr double toggle_height = 32.0;
    constexpr double info_height = 26.0;
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        const auto row_y = panel_y + 42.0 + static_cast<double>(i) * row_height;
        if (pointer_x_ >= panel_x + padding && pointer_x_ <= panel_x + panel_width - padding &&
            pointer_y_ >= row_y + 14.0 && pointer_y_ <= row_y + 40.0) {
            dragging_slider_ = static_cast<int>(i);
            updateDraggedSlider();
            return;
        }
    }

    const auto toggle_y = panel_y + 42.0 + static_cast<double>(sliders_.size()) * row_height;
    for (std::size_t i = 0; i < toggles_.size(); ++i) {
        const auto top = toggle_y + static_cast<double>(i) * toggle_height;
        if (pointer_x_ >= panel_x + padding && pointer_x_ <= panel_x + panel_width - padding &&
            pointer_y_ >= top && pointer_y_ <= top + toggle_height) {
            if (toggles_[i].value != nullptr) *toggles_[i].value = !*toggles_[i].value;
            return;
        }
    }

    const auto info_y = panel_y + 42.0 + static_cast<double>(sliders_.size()) * row_height +
                        static_cast<double>(toggles_.size()) * toggle_height + 4.0;
    const auto button_y = info_y + static_cast<double>(info_labels_.size()) * info_height;
    static_cast<void>(button_y);
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const auto top = button_y + static_cast<double>(i) * 40.0;
        if (pointer_x_ >= panel_x + padding && pointer_x_ <= panel_x + panel_width - padding &&
            pointer_y_ >= top && pointer_y_ <= top + 30.0 && buttons_[i].action) {
            buttons_[i].action();
            return;
        }
    }
}

void UserInterface::updateDraggedSlider() noexcept {
    if (dragging_slider_ < 0 || static_cast<std::size_t>(dragging_slider_) >= sliders_.size()) return;
    constexpr double panel_x = 16.0;
    constexpr double panel_width = 300.0;
    constexpr double padding = 12.0;
    const auto track_left = panel_x + padding;
    const auto track_width = panel_width - padding * 2.0;
    const auto ratio = std::clamp((pointer_x_ - track_left) / track_width, 0.0, 1.0);
    auto& slider = sliders_[static_cast<std::size_t>(dragging_slider_)];
    auto value = slider.minimum + ratio * (slider.maximum - slider.minimum);
    if (slider.integral) value = std::round(value);
    slider.set_value(value);
}

void UserInterface::draw(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return;

    constexpr float panel_x = 16.0F;
    constexpr float panel_y = 16.0F;
    constexpr float panel_width = 300.0F;
    constexpr float padding = 12.0F;
    constexpr float row_height = 52.0F;
    constexpr float toggle_height = 32.0F;
    constexpr float info_height = 26.0F;
    const auto panel_height = 54.0F + static_cast<float>(sliders_.size()) * row_height +
                              static_cast<float>(toggles_.size()) * toggle_height +
                              static_cast<float>(info_labels_.size()) * info_height +
                              static_cast<float>(buttons_.size()) * 40.0F;
    std::vector<Vertex> vertices;
    vertices.reserve(8000);
    addRectangle(vertices, panel_x, panel_y, panel_width, panel_height,
                 {0.025F, 0.038F, 0.065F, 0.94F});
    addRectangle(vertices, panel_x, panel_y, panel_width, 3.0F,
                 {0.20F, 0.67F, 0.95F, 1.0F});
    addText(vertices, panel_title_, panel_x + padding, panel_y + 10.0F, 3.0F,
            {0.88F, 0.94F, 1.0F, 1.0F});

    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        const auto& slider = sliders_[i];
        const auto row_y = panel_y + 42.0F + static_cast<float>(i) * row_height;
        const auto value = std::clamp(slider.get_value(), slider.minimum, slider.maximum);
        const auto ratio = (value - slider.minimum) / (slider.maximum - slider.minimum);
        addText(vertices, slider.label, panel_x + padding, row_y, 3.0F,
                {0.75F, 0.82F, 0.91F, 1.0F});
        const auto value_text = formatValue(value, slider.integral);
        const auto value_x = panel_x + panel_width - padding -
                             static_cast<float>(value_text.size()) * 12.0F;
        addText(vertices, value_text, value_x, row_y, 3.0F,
                {0.92F, 0.96F, 1.0F, 1.0F});

        const auto track_x = panel_x + padding;
        const auto track_y = row_y + 23.0F;
        const auto track_width = panel_width - padding * 2.0F;
        addRectangle(vertices, track_x, track_y, track_width, 7.0F,
                     {0.13F, 0.18F, 0.25F, 1.0F});
        addRectangle(vertices, track_x, track_y, track_width * static_cast<float>(ratio), 7.0F,
                     {0.16F, 0.62F, 0.91F, 1.0F});
        addRectangle(vertices, track_x + track_width * static_cast<float>(ratio) - 4.0F,
                     track_y - 3.0F, 8.0F, 13.0F,
                     {0.78F, 0.91F, 1.0F, 1.0F});
    }

    const auto toggle_y = panel_y + 42.0F + static_cast<float>(sliders_.size()) * row_height;
    for (std::size_t i = 0; i < toggles_.size(); ++i) {
        const auto top = toggle_y + static_cast<float>(i) * toggle_height;
        addText(vertices, toggles_[i].label, panel_x + padding, top + 8.0F, 3.0F,
                {0.75F, 0.82F, 0.91F, 1.0F});
        const auto enabled = toggles_[i].value != nullptr && *toggles_[i].value;
        addRectangle(vertices, panel_x + panel_width - padding - 24.0F, top + 6.0F,
                     20.0F, 20.0F,
                     enabled ? std::array<float, 4>{0.16F, 0.62F, 0.91F, 1.0F}
                             : std::array<float, 4>{0.13F, 0.18F, 0.25F, 1.0F});
        if (enabled) {
            addRectangle(vertices, panel_x + panel_width - padding - 19.0F, top + 11.0F,
                         10.0F, 10.0F, {0.88F, 0.96F, 1.0F, 1.0F});
        }
    }

    const auto info_y = panel_y + 42.0F + static_cast<float>(sliders_.size()) * row_height +
                        static_cast<float>(toggles_.size()) * toggle_height + 4.0F;
    for (std::size_t i = 0; i < info_labels_.size(); ++i) {
        const auto text = info_labels_[i].text ? info_labels_[i].text() : std::string{};
        addText(vertices, text, panel_x + padding,
                info_y + static_cast<float>(i) * info_height, 2.5F,
                {0.62F, 0.75F, 0.88F, 1.0F});
    }

    const auto button_y = info_y + static_cast<float>(info_labels_.size()) * info_height;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const auto top = button_y + static_cast<float>(i) * 40.0F;
        const auto hovered = pointer_x_ >= panel_x + padding &&
                             pointer_x_ <= panel_x + panel_width - padding &&
                             pointer_y_ >= top && pointer_y_ <= top + 30.0F;
        addRectangle(vertices, panel_x + padding, top, panel_width - padding * 2.0F, 30.0F,
                     hovered ? std::array<float, 4>{0.12F, 0.34F, 0.48F, 1.0F}
                             : std::array<float, 4>{0.09F, 0.20F, 0.30F, 1.0F});
        addText(vertices, buttons_[i].label, panel_x + padding + 8.0F, top + 7.0F, 3.0F,
                {0.86F, 0.93F, 1.0F, 1.0F});
    }

    if (!heatmap_legend_.empty() && width >= 230 && height >= 60) {
        constexpr float legend_width = 180.0F;
        constexpr float legend_height = 12.0F;
        const auto legend_x = static_cast<float>(width) - legend_width - 20.0F;
        const auto legend_y = static_cast<float>(height) - 42.0F;
        addText(vertices, heatmap_legend_, legend_x, legend_y - 17.0F, 2.0F,
                {0.80F, 0.87F, 0.96F, 1.0F});
        constexpr int blocks = 45;
        for (int block = 0; block < blocks; ++block) {
            const auto heat = core::heatColor(
                static_cast<float>(block) / static_cast<float>(blocks - 1));
            const auto block_width = legend_width / static_cast<float>(blocks);
            addRectangle(vertices, legend_x + static_cast<float>(block) * block_width,
                         legend_y, block_width + 0.5F, legend_height,
                         {heat[0], heat[1], heat[2], 1.0F});
        }
        addText(vertices, "COOL", legend_x, legend_y + 17.0F, 2.0F,
                {0.58F, 0.67F, 0.78F, 1.0F});
        addText(vertices, "HOT", legend_x + legend_width - 24.0F, legend_y + 17.0F,
                2.0F, {0.58F, 0.67F, 0.78F, 1.0F});
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(resolution_uniform_, static_cast<float>(width), static_cast<float>(height));
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UserInterface::release() noexcept {
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

} // namespace opengl_sim::ui
