#include "opengl_sim/physics/atom.hpp"

#include "opengl_sim/physics/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace opengl_sim::physics {
namespace {

struct subshell {
    int principal;
    int angular;
    int capacity;
};

// Madelung filling order gives a useful first approximation to neutral ground
// state configurations. The model does not include measured shell exceptions.
constexpr std::array subshell_order{
    subshell{1, 0, 2}, subshell{2, 0, 2}, subshell{2, 1, 6},
    subshell{3, 0, 2}, subshell{3, 1, 6}, subshell{4, 0, 2},
    subshell{3, 2, 10}, subshell{4, 1, 6}, subshell{5, 0, 2},
    subshell{4, 2, 10}, subshell{5, 1, 6}, subshell{6, 0, 2},
    subshell{4, 3, 14}, subshell{5, 2, 10}, subshell{6, 1, 6},
    subshell{7, 0, 2}, subshell{5, 3, 14}, subshell{6, 2, 10},
    subshell{7, 1, 6},
};

double factorial(int value) noexcept {
    double result = 1.0;
    for (int factor = 2; factor <= value; ++factor) {
        result *= static_cast<double>(factor);
    }
    return result;
}

double generalizedLaguerre(int order, int alpha, double x) noexcept {
    if (order <= 0) return 1.0;
    auto previous = 1.0;
    auto current = 1.0 + static_cast<double>(alpha) - x;
    for (int index = 2; index <= order; ++index) {
        const auto i = static_cast<double>(index);
        const auto next = ((2.0 * i - 1.0 + static_cast<double>(alpha) - x) * current -
                           (i - 1.0 + static_cast<double>(alpha)) * previous) / i;
        previous = current;
        current = next;
    }
    return current;
}

double radialWavefunctionSquared(const atomic_orbital& orbital, double radius_bohr,
                                double effective_bohr_radius_bohr) noexcept {
    const auto n = orbital.principal_quantum_number;
    const auto l = orbital.angular_momentum_quantum_number;
    const auto z = std::max(orbital.effective_nuclear_charge, 0.01);
    const auto rho = 2.0 * z * std::max(radius_bohr, 0.0) /
                     (static_cast<double>(n) * effective_bohr_radius_bohr);
    const auto radial_order = n - l - 1;
    const auto normalization = std::pow(2.0 * z /
                                             (static_cast<double>(n) * effective_bohr_radius_bohr),
                                         3.0) *
                               factorial(radial_order) /
                               (2.0 * static_cast<double>(n) * factorial(n + l));
    const auto laguerre = generalizedLaguerre(radial_order, 2 * l + 1, rho);
    return normalization * std::exp(-rho) * std::pow(rho, 2 * l) * laguerre * laguerre;
}

} // namespace

int atom::net_charge_number() const noexcept {
    return proton_count - electron_count;
}

double atom::net_charge_coulombs() const noexcept {
    return static_cast<double>(net_charge_number()) * elementary_charge_c;
}

double atom::nucleus_mass_kg() const noexcept {
    return static_cast<double>(std::max(proton_count, 0)) * proton_mass_kg +
           static_cast<double>(std::max(neutron_count, 0)) * neutron_mass_kg;
}

double atom::reduced_bohr_radius_m() const noexcept {
    const auto nucleus_mass = nucleus_mass_kg();
    const auto reduced_mass = electron_mass_kg * nucleus_mass /
                             (electron_mass_kg + nucleus_mass);
    if (reduced_mass <= 0.0) return bohr_radius_m;
    return bohr_radius_m * electron_mass_kg / reduced_mass;
}

std::vector<atomic_orbital> atom::ground_state_orbitals() const {
    std::vector<atomic_orbital> orbitals;
    orbitals.reserve(subshell_order.size());
    auto remaining = std::clamp(electron_count, 0, 118);
    for (const auto& shell : subshell_order) {
        if (remaining <= 0) break;
        const auto occupancy = std::min(remaining, shell.capacity);
        orbitals.push_back({shell.principal, shell.angular, occupancy, 1.0});
        remaining -= occupancy;
    }

    const auto nuclear_charge = static_cast<double>(std::max(proton_count, 1));
    for (std::size_t index = 0; index < orbitals.size(); ++index) {
        auto shielding = 0.0;
        const auto& target = orbitals[index];
        for (std::size_t other_index = 0; other_index < orbitals.size(); ++other_index) {
            const auto& other = orbitals[other_index];
            auto factor = 0.0;
            if (target.angular_momentum_quantum_number <= 1) {
                if (other.principal_quantum_number == target.principal_quantum_number &&
                    other.angular_momentum_quantum_number <= 1) {
                    const auto same_shell_factor = target.principal_quantum_number == 1
                                                       ? 0.30 : 0.35;
                    factor = index == other_index
                                 ? same_shell_factor * static_cast<double>(
                                       std::max(other.electron_occupancy - 1, 0))
                                 : same_shell_factor * static_cast<double>(other.electron_occupancy);
                } else if (other.principal_quantum_number == target.principal_quantum_number - 1) {
                    factor = 0.85 * static_cast<double>(other.electron_occupancy);
                } else if (other.principal_quantum_number < target.principal_quantum_number - 1) {
                    factor = static_cast<double>(other.electron_occupancy);
                }
            } else if (index == other_index) {
                factor = 0.35 * static_cast<double>(std::max(other.electron_occupancy - 1, 0));
            } else if (other.principal_quantum_number < target.principal_quantum_number ||
                       (other.principal_quantum_number == target.principal_quantum_number &&
                        other.angular_momentum_quantum_number < target.angular_momentum_quantum_number)) {
                factor = static_cast<double>(other.electron_occupancy);
            }
            shielding += factor;
        }
        orbitals[index].effective_nuclear_charge = std::max(nuclear_charge - shielding, 0.01);
    }
    return orbitals;
}

double atom::maximum_orbital_radius_bohr() const {
    const auto orbitals = ground_state_orbitals();
    auto maximum = 1.0;
    for (const auto& orbital : orbitals) {
        const auto n = static_cast<double>(orbital.principal_quantum_number);
        maximum = std::max(maximum, 18.0 * n * n /
                                        std::max(orbital.effective_nuclear_charge, 0.01));
    }
    return maximum;
}

double atom::electron_density_per_bohr_cubed(double radius_bohr) const {
    if (electron_count <= 0) return 0.0;
    return electron_density_per_bohr_cubed(radius_bohr, ground_state_orbitals());
}

double atom::electron_density_per_bohr_cubed(
    double radius_bohr, const std::vector<atomic_orbital>& orbitals) const noexcept {
    if (electron_count <= 0) return 0.0;
    const auto reduced_radius = reduced_bohr_radius_m() / bohr_radius_m;
    auto density = 0.0;
    for (const auto& orbital : orbitals) {
        density += static_cast<double>(orbital.electron_occupancy) *
                   radialWavefunctionSquared(orbital, radius_bohr, reduced_radius) /
                   (4.0 * std::numbers::pi_v<double>);
    }
    return density;
}

double atom::orbital_radial_probability(const atomic_orbital& orbital,
                                       double radius_bohr) const noexcept {
    const auto reduced_radius = reduced_bohr_radius_m() / bohr_radius_m;
    return radius_bohr * radius_bohr *
           radialWavefunctionSquared(orbital, radius_bohr, reduced_radius);
}

} // namespace opengl_sim::physics
