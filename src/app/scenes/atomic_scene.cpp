#include "opengl_sim/app/scenes/atomic_scene.hpp"

#include "opengl_sim/render/renderer.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace opengl_sim::app {
namespace {

constexpr std::array<const char*, 4> atom_preset_names{
    "HYDROGEN-1", "HELIUM-4", "CARBON-12", "OXYGEN-16"};

std::string formatDecimal(double value, int precision) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

} // namespace

void AtomicScene::loadPreset(int preset) noexcept {
    selected_preset_ = std::clamp(preset, 0, 3);
    switch (selected_preset_) {
    case 0:
        atom_.proton_count = 1;
        atom_.neutron_count = 0;
        atom_.electron_count = 1;
        break;
    case 1:
        atom_.proton_count = 2;
        atom_.neutron_count = 2;
        atom_.electron_count = 2;
        break;
    case 2:
        atom_.proton_count = 6;
        atom_.neutron_count = 6;
        atom_.electron_count = 6;
        break;
    default:
        atom_.proton_count = 8;
        atom_.neutron_count = 8;
        atom_.electron_count = 8;
        break;
    }
    atom_.visual_pattern = physics::electron_pattern::spherical;
    atom_.photon_active = false;
    atom_.photon_time_fs = 0.0;
    atom_.photon_amplitude = {1.0, 0.0};
    atom_.excited_amplitude = {0.0, 0.0};
}

void AtomicScene::configureUI(ui::UserInterface& interface) {
    interface.addButton("NEXT ATOM MODEL", [this] { loadPreset((selected_preset_ + 1) % 4); });
    interface.addSlider("ELECTRONS", atom_.electron_count, 0, 118);
    interface.addSlider("ELECTRON SIZE", atom_.electron_size, 1.0, 12.0);
    interface.addSlider("PROTONS", atom_.proton_count, 1, 118);
    interface.addSlider("NEUTRONS", atom_.neutron_count, 0, 300);
    interface.addSlider("PHOTON ENERGY EV", [this] { return atom_.photon_energy_ev; },
                        [this](double value) { atom_.setPhotonEnergyEv(value); }, 1.0, 20.0);
    interface.addSlider("FIELD COUPLING EV", [this] { return atom_.photon_coupling_ev; },
                        [this](double value) { atom_.setPhotonCouplingEv(value); }, 0.0, 2.5);
    interface.addInfo([this] {
        const auto charge = atom_.net_charge_number();
        return std::string{"NET CHARGE "} + (charge >= 0 ? "+" : "") +
               std::to_string(charge) + "E";
    });
    interface.addInfo([this] {
        return std::string{"NUCLEUS P"} + std::to_string(atom_.proton_count) +
               " N" + std::to_string(atom_.neutron_count) + " / E" +
               std::to_string(atom_.electron_count);
    });
    interface.addInfo([this] {
        if (atom_.electron_count == 0) return std::string{"NO ELECTRON CLOUD"};
        if (atom_.electron_count > atom_.proton_count) {
            return std::string{"NEGATIVE ION IS APPROXIMATE"};
        }
        return std::string{"SCREENED GROUND STATE"};
    });
    interface.addInfo([this] {
        return std::string{"L"} + formatDecimal(atom_.photon_wavelength_nm(), 1) +
               "NM  F" + formatDecimal(atom_.photon_frequency_hz() / 1e12, 0) + "THZ";
    });
    interface.addInfo([this] {
        return std::string{"1S-2P GAP "} + formatDecimal(atom_.transition_energy_ev(), 2) + "EV";
    });
    interface.addInfo([this] {
        return std::string{"EXCITED "} +
               formatDecimal(atom_.excitation_probability() * 100.0, 1) +
               "%  E " + formatDecimal(atom_.atomic_energy_ev(), 2) + "EV";
    });
    interface.addInfo([this] {
        switch (atom_.visual_pattern) {
        case physics::electron_pattern::p_orbital: return std::string{"P ORBITAL LOBES"};
        case physics::electron_pattern::d_orbital: return std::string{"D ORBITAL LOBES"};
        default: return std::string{"SPHERICAL ORBITAL CLOUD"};
        }
    });
    interface.addInfo([this] {
        if (!atom_.photon_active && atom_.photon_time_fs == 0.0) return std::string{"PHOTON READY"};
        if (atom_.photon_active) return std::string{"PHOTON WAVE IN FLIGHT"};
        return atom_.photon_survival_probability() < 0.5
            ? std::string{"PHOTON MOSTLY ABSORBED"}
            : std::string{"PHOTON MOSTLY TRANSMITTED"};
    });
    interface.addInfo([this] {
        return std::string{"MODEL "} + atom_preset_names[static_cast<std::size_t>(selected_preset_)];
    });
    interface.setHeatmapLegend("ELECTRON PROBABILITY");
    interface.addButton("EMIT PHOTON", [this] { atom_.emitPhoton(); });
    interface.addButton("ADD PROTON", [this] {
        atom_.proton_count = std::min(atom_.proton_count + 1, 118);
    });
    interface.addButton("NEXT ELECTRON PATTERN", [this] {
        switch (atom_.visual_pattern) {
        case physics::electron_pattern::spherical:
            atom_.visual_pattern = physics::electron_pattern::p_orbital;
            break;
        case physics::electron_pattern::p_orbital:
            atom_.visual_pattern = physics::electron_pattern::d_orbital;
            break;
        case physics::electron_pattern::d_orbital:
            atom_.visual_pattern = physics::electron_pattern::spherical;
            break;
        }
    });
}

void AtomicScene::update(double delta_seconds) {
    atom_.updatePhoton(delta_seconds);
}

void AtomicScene::draw(render::Renderer& renderer, const render::Camera& camera,
                       int width, int height) {
    renderer.drawAtomic(atom_, camera, width, height);
}

} // namespace opengl_sim::app
