#pragma once

#include <vector>

namespace opengl_sim::physics {

struct atomic_orbital {
    int principal_quantum_number = 1;
    int angular_momentum_quantum_number = 0;
    int electron_occupancy = 0;
    double effective_nuclear_charge = 1.0;
};

// A nucleus plus a ground-state, spherically averaged electron configuration.
// Electron coordinates are represented by probability density, not classical
// trajectories. Multi-electron orbital sizes use a screened Coulomb model.
struct atom {
    int proton_count = 1;
    int neutron_count = 0;
    int electron_count = 1;

    [[nodiscard]] int net_charge_number() const noexcept;
    [[nodiscard]] double net_charge_coulombs() const noexcept;
    [[nodiscard]] double nucleus_mass_kg() const noexcept;
    [[nodiscard]] double reduced_bohr_radius_m() const noexcept;
    [[nodiscard]] std::vector<atomic_orbital> ground_state_orbitals() const;
    [[nodiscard]] double maximum_orbital_radius_bohr() const;
    [[nodiscard]] double electron_density_per_bohr_cubed(double radius_bohr) const;
    [[nodiscard]] double electron_density_per_bohr_cubed(
        double radius_bohr, const std::vector<atomic_orbital>& orbitals) const noexcept;
    [[nodiscard]] double orbital_radial_probability(
        const atomic_orbital& orbital, double radius_bohr) const noexcept;
};

} // namespace opengl_sim::physics
