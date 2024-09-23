#include <glm/glm.hpp>

struct Camera {
  glm::vec3 pos;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 target;
  glm::vec3 velocity;
};
