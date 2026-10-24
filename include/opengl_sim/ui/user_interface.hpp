#pragma once

#include "opengl_sim/core/logger.hpp"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace opengl_sim::ui {

class UserInterface {
public:
    explicit UserInterface(core::Logger& logger) noexcept;
    ~UserInterface();

    UserInterface(const UserInterface&) = delete;
    UserInterface& operator=(const UserInterface&) = delete;

    template <typename Value>
    void addSlider(std::string label, Value& value, Value minimum, Value maximum) {
        static_assert(std::is_arithmetic_v<Value>, "UI sliders require a numeric value.");
        addSliderControl(
            std::move(label),
            [&value] { return static_cast<double>(value); },
            [&value](double next) { value = static_cast<Value>(next); },
            static_cast<double>(minimum), static_cast<double>(maximum),
            std::is_integral_v<Value>);
    }

    void addButton(std::string label, std::function<void()> action);
    [[nodiscard]] bool initialize();
    void setPointerPosition(double x, double y) noexcept;
    [[nodiscard]] bool pointerOverPanel() const noexcept;
    void onPrimaryButton(bool pressed);
    void draw(int width, int height);

private:
    struct Slider {
        std::string label;
        std::function<double()> get_value;
        std::function<void(double)> set_value;
        double minimum = 0.0;
        double maximum = 1.0;
        bool integral = false;
    };

    struct Button {
        std::string label;
        std::function<void()> action;
    };

    void addSliderControl(std::string label, std::function<double()> get_value,
                          std::function<void(double)> set_value, double minimum,
                          double maximum, bool integral);
    void updateDraggedSlider() noexcept;
    void release() noexcept;

    core::Logger& logger_;
    std::vector<Slider> sliders_;
    std::vector<Button> buttons_;
    double pointer_x_ = 0.0;
    double pointer_y_ = 0.0;
    int dragging_slider_ = -1;
    unsigned int program_ = 0;
    unsigned int vertex_array_ = 0;
    unsigned int vertex_buffer_ = 0;
    int resolution_uniform_ = -1;
    bool initialized_ = false;
};

} // namespace opengl_sim::ui
