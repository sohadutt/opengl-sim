#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace opengl_sim::core {

[[nodiscard]] inline std::array<float, 3> heatColor(float value) noexcept {
    constexpr std::array<std::array<float, 3>, 5> colors{{
        {0.38F, 0.025F, 0.008F},
        {1.0F, 0.19F, 0.025F},
        {1.0F, 0.42F, 0.08F},
        {1.0F, 0.67F, 0.20F},
        {1.0F, 0.94F, 0.78F},
    }};
    const auto scaled = std::clamp(value, 0.0F, 1.0F) * 4.0F;
    const auto index = std::min(static_cast<std::size_t>(scaled), colors.size() - 2);
    const auto fraction = scaled - static_cast<float>(index);
    return {
        colors[index][0] + (colors[index + 1][0] - colors[index][0]) * fraction,
        colors[index][1] + (colors[index + 1][1] - colors[index][1]) * fraction,
        colors[index][2] + (colors[index + 1][2] - colors[index][2]) * fraction,
    };
}

} // namespace opengl_sim::core
