#pragma once
#include <glm/glm.hpp>

namespace opengl_sim::physics {
struct atom {
  int proton;
  int neutron;
  glm::dvec3 position;
  struct electron {
      int count;
      glm::dvec3 position;
  };
  [[nodiscard]] std::vector<glm::dvec3> simulate_atom() const;
};
} // namespace opengl_sim::physics
