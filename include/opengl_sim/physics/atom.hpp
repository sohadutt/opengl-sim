#pragma once

#include "opengl_sim/physics/constants.hpp"

#include <complex>
#include <vector>

namespace opengl_sim::physics {

struct atomic_orbital {
    int principal_quantum_number = 1;
    int angular_momentum_quantum_number = 0;
    int electron_occupancy = 0;
    double effective_nuclear_charge = 1.0;
};

// A nucleus plus a ground-state, spherically averaged electron configuration
// and an optional driven hydrogenic 1s/2p single-photon interaction. Electron
// coordinates are represented by probability density, not classical paths.
struct atom {
    int proton_count = 1;
    int neutron_count = 0;
    int electron_count = 1;
    double electron_size = 3.3; // OpenGL point size; does not affect the atom model.

    double photon_energy_ev = hydrogen_1s_2p_transition_ev;
    double photon_coupling_ev = 1.0;
    double photon_time_fs = 0.0;
    std::complex<double> photon_amplitude{1.0, 0.0};
    std::complex<double> excited_amplitude{0.0, 0.0};
    bool photon_active = false;

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

    void setPhotonEnergyEv(double energy_ev) noexcept;
    void setPhotonCouplingEv(double coupling_ev) noexcept;
    void emitPhoton() noexcept;
    void updatePhoton(double delta_seconds) noexcept;

    [[nodiscard]] double transition_energy_ev() const;
    [[nodiscard]] double photon_frequency_hz() const noexcept;
    [[nodiscard]] double photon_wavelength_nm() const noexcept;
    [[nodiscard]] double excitation_probability() const noexcept;
    [[nodiscard]] double photon_survival_probability() const noexcept;
    [[nodiscard]] double atomic_energy_ev() const;
    [[nodiscard]] double photon_center_x() const noexcept;
    [[nodiscard]] double photon_visual_wave_number() const noexcept;
    [[nodiscard]] bool photon_visible() const noexcept;
};

} // namespace opengl_sim::physics
