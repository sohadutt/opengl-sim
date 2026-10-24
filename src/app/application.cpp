#include "opengl_sim/app/application.hpp"
#include "opengl_sim/physics/atom.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/physics/constants.hpp"
#include "opengl_sim/physics/grid_plane.hpp"
#include "opengl_sim/physics/light_particle_field.hpp"
#include "opengl_sim/physics/planet.hpp"
#include "opengl_sim/render/renderer.hpp"
#include "opengl_sim/ui/user_interface.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <random>



namespace opengl_sim::app {
namespace {

enum class SimulationMode { selection, celestial, atomic };

core::Logger *active_logger = nullptr;

glm::vec3 randomPlanetColor() {
  static std::mt19937 random_engine{std::random_device{}()};
  static std::uniform_real_distribution<float> channel{0.15F, 0.95F};
  return {channel(random_engine), channel(random_engine), channel(random_engine)};
}

void glfwErrorCallback(int error, const char *description) {
  if (active_logger != nullptr) {
    active_logger->error("GLFW error " + std::to_string(error) + ": " +
                         description);
  }
}

void keyCallback(GLFWwindow *window, int key, int, int action, int) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

struct WindowInput {
  render::Camera *camera = nullptr;
  ui::UserInterface *interface = nullptr;
  int look_button = -1;
};

void mouseMoveCallback(GLFWwindow *window, double x_position, double y_position) {
  auto *input = static_cast<WindowInput *>(glfwGetWindowUserPointer(window));
  if (input == nullptr) return;
  input->interface->setPointerPosition(x_position, y_position);
  if (input->look_button != -1) {
    input->camera->processMouseMovement(x_position, y_position);
  }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  auto *input = static_cast<WindowInput *>(glfwGetWindowUserPointer(window));
  if (input == nullptr) return;
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    input->interface->onPrimaryButton(action == GLFW_PRESS);
  }

  if (action == GLFW_PRESS &&
      (button == GLFW_MOUSE_BUTTON_RIGHT ||
       (button == GLFW_MOUSE_BUTTON_LEFT && !input->interface->pointerOverPanel()))) {
    input->look_button = button;
    input->camera->resetMouseTracking();
  } else if (action == GLFW_RELEASE && input->look_button == button) {
    input->look_button = -1;
  }
}

void scrollCallback(GLFWwindow *window, double x_offset, double y_offset) {
  auto *input = static_cast<WindowInput *>(glfwGetWindowUserPointer(window));
  if (input == nullptr || input->interface->pointerOverPanel()) return;

  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
    input->camera->move({static_cast<float>(-x_offset), static_cast<float>(y_offset), 0.0F},
                        0.12);
  } else {
    input->camera->zoomFromScroll(y_offset);
  }
}

class GlfwSession {
public:
  explicit GlfwSession(core::Logger &logger) : logger_(logger) {
    active_logger = &logger_;
    glfwSetErrorCallback(glfwErrorCallback);
    initialized_ = glfwInit() == GLFW_TRUE;
  }

  ~GlfwSession() {
    if (initialized_) {
      glfwTerminate();
    }
    glfwSetErrorCallback(nullptr);
    active_logger = nullptr;
  }

  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
  core::Logger &logger_;
  bool initialized_ = false;
};

} // namespace

Application::Application(const core::Settings &settings,
                         core::Logger &logger) noexcept
    : settings_(settings), logger_(logger) {}

int Application::run() {
  GlfwSession glfw{logger_};
  if (!glfw.initialized()) {
    logger_.critical("GLFW initialization failed.");
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(settings_.window_width, settings_.window_height,
                       settings_.window_title.c_str(), nullptr, nullptr);
  if (window == nullptr) {
    logger_.critical("Could not create the OpenGL window.");
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(settings_.vertical_sync ? 1 : 0);
  glfwSetKeyCallback(window, keyCallback);

  int result = EXIT_SUCCESS;
  {
    render::Renderer renderer{logger_};
    ui::UserInterface interface{logger_};
    if (!renderer.initialize() || !interface.initialize()) {
      result = EXIT_FAILURE;
    } else {
      // Initial visual scale in simulation units; the UI can make this adjustable later.
      physics::black_hole black_hole{{0.0, 0.0, 0.0}, 1.0, 0.24, 0.5};
      physics::light_particle_field particles;
      physics::atom atom_model;
      std::vector<physics::planet> planets;
      physics::planet first_planet{{0.35, 0.12, -2.2}, 0.52, {0.16F, 0.58F, 0.95F}};
      first_planet.velocity = {0.37, 0.0, 0.06};
      planets.push_back(first_planet);
      std::size_t selected_planet = 0;
      constexpr std::size_t maximum_planets = 32;
      physics::grid_plane grid{{0.0, 0.0, 0.0}, 1.0, 10.0, 0.01, 0.5};
      bool show_grid = true;
      render::Camera camera;
      auto current_mode = SimulationMode::selection;
      auto requested_mode = SimulationMode::selection;
      const auto configure_interface = [&] {
        interface.clearControls();
        if (current_mode == SimulationMode::selection) {
          interface.setPanelTitle("CHOOSE SIMULATION");
          interface.addInfo([] { return "SELECT A MODE TO START"; });
          interface.addButton("CELESTIAL MODE", [&requested_mode] {
            requested_mode = SimulationMode::celestial;
          });
          interface.addButton("ATOMIC MODE", [&requested_mode] {
            requested_mode = SimulationMode::atomic;
          });
        } else if (current_mode == SimulationMode::celestial) {
          interface.setPanelTitle("CELESTIAL MODE");
          interface.addSlider("PLANET SIZE", [&] {
            return planets.empty() ? 0.52 : planets[selected_planet].radius;
          }, [&](double value) {
            if (!planets.empty()) planets[selected_planet].radius = value;
          }, 0.1, 2.0);
          interface.addSlider("PLANET MASS", [&] {
            return planets.empty() ? 0.04 : planets[selected_planet].mass;
          }, [&](double value) {
            if (!planets.empty()) planets[selected_planet].mass = value;
          }, 0.005, 0.25);
          interface.addInfo([&] {
            return planets.empty() ? std::string{"NO PLANETS"} :
                std::string{"PLANET "} + std::to_string(selected_planet + 1) + "/" +
                    std::to_string(planets.size());
          });
          interface.addButton("ADD PLANET", [&] {
            if (planets.size() >= maximum_planets) return;
            const auto index = planets.size();
            const auto angle = static_cast<double>(index) * 2.399963229728653;
            const auto distance = 2.2 + 0.35 * static_cast<double>(index % 6);
            physics::planet body;
            body.position = black_hole.position + glm::dvec3{
                std::cos(angle) * distance, 0.15 * static_cast<double>(index % 3),
                std::sin(angle) * distance};
            const auto orbital_speed = std::sqrt(physics::scaled_celestial_G *
                                                 std::max(black_hole.mass, 0.05) / distance);
            body.velocity = {-std::sin(angle) * orbital_speed, 0.0,
                             std::cos(angle) * orbital_speed};
            body.color = randomPlanetColor();
            planets.push_back(body);
            selected_planet = planets.size() - 1;
          });
          interface.addButton("NEXT PLANET", [&] {
            if (!planets.empty()) selected_planet = (selected_planet + 1) % planets.size();
          });
          interface.addButton("CHANGE PLANET COLOUR", [&] {
            if (!planets.empty()) planets[selected_planet].color = randomPlanetColor();
          });
          interface.addSlider("GRID SPACING", grid.spacing, 0.1, 5.0);
          interface.addSlider("GRID SIZE", grid.size, 1.0, 20.0);
          interface.addSlider("GRID THICKNESS", grid.line_thickness, 0.002, 0.05);
          interface.addSlider("GRID OPACITY", grid.line_opacity, 0.05, 1.0);
          interface.addSlider("PARTICLE COUNT", particles.count, 1, 1'000'000);
          interface.addSlider("PARTICLE SPEED", particles.speed, 0.1, 6.0);
          interface.addSlider("PARTICLE SIZE", particles.size, 1.0, 12.0);
          interface.addToggle("TRAILS", particles.trails_enabled);
          interface.addSlider("MASS", black_hole.mass, 0.1, 10.0);
          interface.addSlider("SPIN", black_hole.spin, -1.0, 1.0);
          interface.addSlider("EVENT HORIZON", black_hole.event_horizon, 0.05, 0.8);
          interface.addToggle("GRID", show_grid);
          interface.setHeatmapLegend("PARTICLE HEAT");
          interface.addButton("RESET BLACK HOLE", [&black_hole] {
            black_hole.mass = 1.0;
            black_hole.spin = 0.5;
            black_hole.event_horizon = 0.24;
            black_hole.velocity = {};
          });
        } else {
          interface.setPanelTitle("ATOMIC MODE");
          interface.addSlider("ELECTRONS", atom_model.electron_count, 0, 118);
          interface.addSlider("PROTONS", atom_model.proton_count, 1, 118);
          interface.addSlider("NEUTRONS", atom_model.neutron_count, 0, 300);
          interface.addInfo([] { return "ELECTRON -1.60218E-19C"; });
          interface.addInfo([] { return "PROTON +1.60218E-19C"; });
          interface.addInfo([] { return "NEUTRON 0C"; });
          interface.addInfo([&atom_model] {
            const auto charge = atom_model.net_charge_number();
            return std::string{"NET CHARGE "} + (charge >= 0 ? "+" : "") +
                   std::to_string(charge) + "E";
          });
          interface.addInfo([&atom_model] {
            if (atom_model.electron_count == 0) return std::string{"NO ELECTRON CLOUD"};
            if (atom_model.electron_count > atom_model.proton_count) {
              return std::string{"NEGATIVE ION IS APPROXIMATE"};
            }
            return std::string{"SCREENED GROUND STATE"};
          });
          interface.addInfo([] { return "CLOUD IS A PROBABILITY"; });
          interface.addInfo([] { return "NUCLEUS IS NOT TO SCALE"; });
          interface.setHeatmapLegend("ELECTRON PROBABILITY");
          interface.addButton("BACK TO MODE SELECT", [&requested_mode] {
            requested_mode = SimulationMode::selection;
          });
        }
      };
      configure_interface();

      WindowInput input{&camera, &interface};
      glfwSetWindowUserPointer(window, &input);
      glfwSetCursorPosCallback(window, mouseMoveCallback);
      glfwSetMouseButtonCallback(window, mouseButtonCallback);
      glfwSetScrollCallback(window, scrollCallback);
      auto previous_frame_time = glfwGetTime();
      logger_.info("Choose celestial or atomic mode in the panel. Drag on the scene to look, scroll to zoom, and Shift+scroll to pan.");

      while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
        if (requested_mode != current_mode) {
          current_mode = requested_mode;
          configure_interface();
        }
        const auto current_frame_time = glfwGetTime();
        const auto delta_seconds = std::min(current_frame_time - previous_frame_time, 0.1);
        previous_frame_time = current_frame_time;

        glm::vec3 movement{0.0F, 0.0F, 0.0F};
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement.x -= 1.0F;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement.x += 1.0F;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) movement.y -= 1.0F;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) movement.y += 1.0F;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement.z -= 1.0F;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement.z += 1.0F;
        camera.move(movement, delta_seconds);

        double zoom_adjustment = 0.0;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
          zoom_adjustment += 1.5;
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
          zoom_adjustment -= 1.5;
        }
        camera.adjustZoom(zoom_adjustment, delta_seconds);
        if (current_mode == SimulationMode::celestial) {
          particles.update(delta_seconds);
          physics::advanceCelestialSystem(black_hole, planets, delta_seconds);
          if (planets.empty()) selected_planet = 0;
          else if (selected_planet >= planets.size()) selected_planet = planets.size() - 1;
        }

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (current_mode == SimulationMode::selection) {
          renderer.drawSelection(framebuffer_width, framebuffer_height);
        } else if (current_mode == SimulationMode::atomic) {
          renderer.drawAtomic(atom_model, camera, framebuffer_width, framebuffer_height);
        } else {
          renderer.draw(black_hole, grid, show_grid, particles, planets, camera,
                        framebuffer_width, framebuffer_height);
        }
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(window, &window_width, &window_height);
        interface.draw(window_width, window_height);
        glfwSwapBuffers(window);
      }
    }
  }

  glfwDestroyWindow(window);
  logger_.info("Application shutting down.");
  return result;
}

} // namespace opengl_sim::app
