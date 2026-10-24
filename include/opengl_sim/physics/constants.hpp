#pragma once

namespace opengl_sim::physics {

inline constexpr double G = 6.67430e-11; // m^3 kg^-1 s^-2
inline constexpr double c = 299'792'458.0;
inline constexpr double scaled_celestial_G = 0.32;

// CODATA 2022 values. Charges are exact in SI; masses and the Bohr radius are
// rounded to the digits useful for this visualization.
inline constexpr double elementary_charge_c = 1.602176634e-19;
inline constexpr double electron_mass_kg = 9.1093837139e-31;
inline constexpr double proton_mass_kg = 1.67262192595e-27;
inline constexpr double neutron_mass_kg = 1.67492750056e-27;
inline constexpr double bohr_radius_m = 5.29177210544e-11;

} // namespace opengl_sim::physics
