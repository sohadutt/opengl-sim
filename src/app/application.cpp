#include "opengl_sim/app/application.hpp"

#include "opengl_sim/app/scene_manager.hpp"
#include "opengl_sim/app/scenes/atomic_scene.hpp"
#include "opengl_sim/app/scenes/celestial_scene.hpp"
#include "opengl_sim/render/camera.hpp"
#include "opengl_sim/render/renderer.hpp"
#include "opengl_sim/ui/user_interface.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace opengl_sim::app {
namespace {

core::Logger* active_logger = nullptr;

void glfwErrorCallback(int error, const char* description) {
    if (active_logger != nullptr) {
        active_logger->error("GLFW error " + std::to_string(error) + ": " + description);
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

struct WindowInput {
    render::Camera* camera = nullptr;
    ui::UserInterface* interface = nullptr;
    int look_button = -1;
};

void mouseMoveCallback(GLFWwindow* window, double x_position, double y_position) {
    auto* input = static_cast<WindowInput*>(glfwGetWindowUserPointer(window));
    if (input == nullptr) return;
    input->interface->setPointerPosition(x_position, y_position);
    if (input->look_button != -1) {
        input->camera->processMouseMovement(x_position, y_position);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    auto* input = static_cast<WindowInput*>(glfwGetWindowUserPointer(window));
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

void scrollCallback(GLFWwindow* window, double x_offset, double y_offset) {
    auto* input = static_cast<WindowInput*>(glfwGetWindowUserPointer(window));
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
    explicit GlfwSession(core::Logger& logger) : logger_(logger) {
        active_logger = &logger_;
        glfwSetErrorCallback(glfwErrorCallback);
        initialized_ = glfwInit() == GLFW_TRUE;
    }

    ~GlfwSession() {
        if (initialized_) glfwTerminate();
        glfwSetErrorCallback(nullptr);
        active_logger = nullptr;
    }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
    core::Logger& logger_;
    bool initialized_ = false;
};

} // namespace

Application::Application(const core::Settings& settings, core::Logger& logger) noexcept
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

    GLFWwindow* window = glfwCreateWindow(settings_.window_width, settings_.window_height,
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
            render::Camera camera;
            SceneManager scenes{interface, logger_};
            scenes.registerScene(std::make_unique<CelestialScene>(logger_));
            scenes.registerScene(std::make_unique<AtomicScene>());
            scenes.configureUI();

            WindowInput input{&camera, &interface};
            glfwSetWindowUserPointer(window, &input);
            glfwSetCursorPosCallback(window, mouseMoveCallback);
            glfwSetMouseButtonCallback(window, mouseButtonCallback);
            glfwSetScrollCallback(window, scrollCallback);

            auto previous_frame_time = glfwGetTime();
            logger_.info("Choose a scene in the panel. Drag on the scene to look, scroll to zoom, and Shift+scroll to pan.");
            while (glfwWindowShouldClose(window) == GLFW_FALSE) {
                glfwPollEvents();
                scenes.processSceneChange();

                const auto current_frame_time = glfwGetTime();
                const auto delta_seconds = std::min(current_frame_time - previous_frame_time, 0.1);
                previous_frame_time = current_frame_time;

                glm::vec3 movement{0.0F};
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement.x -= 1.0F;
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement.x += 1.0F;
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) movement.y -= 1.0F;
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) movement.y += 1.0F;
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement.z -= 1.0F;
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement.z += 1.0F;
                camera.move(movement, delta_seconds);

                double zoom_adjustment = 0.0;
                if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) zoom_adjustment += 1.5;
                if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) zoom_adjustment -= 1.5;
                camera.adjustZoom(zoom_adjustment, delta_seconds);

                scenes.update(delta_seconds);
                int framebuffer_width = 0;
                int framebuffer_height = 0;
                glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
                scenes.draw(renderer, camera, framebuffer_width, framebuffer_height);

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
