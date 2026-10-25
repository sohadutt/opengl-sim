#include "opengl_sim/physics/atom.hpp"

#include "opengl_sim/physics/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace opengl_sim::physics {
namespace {

constexpr auto visual_speed_bohr_per_fs = 4.0;
constexpr auto photon_initial_x_bohr = -5.0;

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

void atom::setPhotonEnergyEv(double energy_ev) noexcept {
    if (std::isfinite(energy_ev)) photon_energy_ev = std::clamp(energy_ev, 0.1, 100.0);
}

void atom::setPhotonCouplingEv(double coupling_ev) noexcept {
    if (std::isfinite(coupling_ev)) photon_coupling_ev = std::clamp(coupling_ev, 0.0, 4.0);
}

void atom::emitPhoton() noexcept {
    photon_time_fs = 0.0;
    photon_amplitude = {1.0, 0.0};
    excited_amplitude = {0.0, 0.0};
    photon_active = true;
}

void atom::updatePhoton(double delta_seconds) noexcept {
    if (!photon_active || !std::isfinite(delta_seconds) || delta_seconds <= 0.0) {
        return;
    }

    // The visual clock runs at 3 fs per wall-clock second. Short exact
    // propagator steps keep the time-dependent pulse envelope well resolved.
    const auto elapsed_fs = std::min(delta_seconds, 0.1) * 3.0;
    constexpr auto maximum_step_fs = 0.02;
    const auto steps = std::max(1, static_cast<int>(std::ceil(elapsed_fs / maximum_step_fs)));
    const auto step_fs = elapsed_fs / static_cast<double>(steps);
    const auto detuning_ev = photon_energy_ev - transition_energy_ev();
    constexpr auto pulse_width_bohr = 1.6;

    for (int step = 0; step < steps; ++step) {
        const auto center_x = photon_center_x();
        const auto envelope = std::exp(-0.5 * center_x * center_x /
                                       (pulse_width_bohr * pulse_width_bohr));
        const auto coupling_ev = electron_count > 0 && proton_count > 0
                                     ? photon_coupling_ev * envelope : 0.0;

        // Exact exponential for a constant two-state Hamiltonian over this
        // short interval: H = [[0, g], [g, detuning]]. This is the rotating-
        // frame, rotating-wave approximation to i*hbar*d|psi>/dt = H|psi>.
        const auto half_detuning = 0.5 * detuning_ev;
        const auto frequency_ev = std::hypot(half_detuning, coupling_ev);
        const auto angle = frequency_ev * step_fs / hbar_ev_fs;
        const auto cosine = std::cos(angle);
        const auto sine_factor = frequency_ev > 1e-14
                                     ? std::sin(angle) / frequency_ev : step_fs / hbar_ev_fs;
        const auto ground = photon_amplitude;
        const auto excited = excited_amplitude;
        const std::complex<double> imaginary{0.0, 1.0};
        photon_amplitude = cosine * ground - imaginary * sine_factor *
            (-half_detuning * ground + coupling_ev * excited);
        excited_amplitude = cosine * excited - imaginary * sine_factor *
            (coupling_ev * ground + half_detuning * excited);

        const auto norm = std::sqrt(std::norm(photon_amplitude) +
                                    std::norm(excited_amplitude));
        if (norm > 0.0) {
            photon_amplitude /= norm;
            excited_amplitude /= norm;
        }
        photon_time_fs += step_fs;
    }

    if (photon_center_x() > 5.0) photon_active = false;
}

double atom::transition_energy_ev() const {
    if (proton_count <= 0 || electron_count <= 0) return 0.0;
    const auto orbitals = ground_state_orbitals();
    const auto effective_charge = orbitals.empty()
                                      ? static_cast<double>(proton_count)
                                      : orbitals.front().effective_nuclear_charge;
    return hydrogen_1s_2p_transition_ev * effective_charge * effective_charge;
}

double atom::photon_frequency_hz() const noexcept {
    return photon_energy_ev * elementary_charge_c / planck_constant_j_s;
}

double atom::photon_wavelength_nm() const noexcept {
    const auto frequency = photon_frequency_hz();
    return frequency > 0.0 ? c / frequency * 1e9 : 0.0;
}

double atom::excitation_probability() const noexcept {
    return std::clamp(std::norm(excited_amplitude), 0.0, 1.0);
}

double atom::photon_survival_probability() const noexcept {
    return std::clamp(std::norm(photon_amplitude), 0.0, 1.0);
}

double atom::atomic_energy_ev() const {
    return excitation_probability() * transition_energy_ev();
}

double atom::photon_center_x() const noexcept {
    return photon_initial_x_bohr + visual_speed_bohr_per_fs * photon_time_fs;
}

double atom::photon_visual_wave_number() const noexcept {
    return photon_energy_ev / hbar_ev_fs / visual_speed_bohr_per_fs;
}

bool atom::photon_visible() const noexcept {
    return photon_active && photon_survival_probability() > 1e-4;
}

} // namespace opengl_sim::physics
