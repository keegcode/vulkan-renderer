#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#include "vk-engine.hpp"

std::vector<Vertex> vertices = {
    {{-1.0f,-1.0f,-1.0f}, {1.0f, 0.0f, 0.0f},{1.0f, 0.0f}},
    {{-1.0f, 1.0f,-1.0f}, {0.0f, 1.0f, 0.0f},{0.0f, 0.0f}},
    {{ 1.0f, 1.0f,-1.0f}, {0.0f, 0.0f, 1.0f},{0.0f, 1.0f}}, 
    {{ 1.0f,-1.0f,-1.0f}, {1.0f, 1.0f, 1.0f},{1.0f, 1.0f}},
    {{ 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f},{1.0f, 0.0f}},
    {{ 1.0f,-1.0f, 1.0f}, {0.0f, 1.0f, 0.0f},{0.0f, 0.0f}},
    {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f},{0.0f, 1.0f}},
    {{-1.0f,-1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},{1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
  0, 1, 2,
  0, 2, 3,
  3, 2, 4,
  3, 4, 5,
  5, 4, 6,
  5, 6, 7,
  7, 6, 1,
  7, 1, 0,
  1, 6, 4,
  1, 4, 2,
  5, 7, 0,
  5, 0, 3
};


int main() {
  Display display{};
  display.init();

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  
  model = glm::rotate(model, glm::radians(45.0f), glm::vec3{1.0f, 1.0f, 0.0f});
  view = glm::translate(view, glm::vec3{0.0f, 0.0f, -3.0f});
  proj = glm::perspective(glm::radians(80.0f), display.displayMode.w / (float) display.displayMode.h, 0.1f, 10.0f);

  proj[1][1] *= -1;

  Transform transform{model, view, proj};

  VkEngine engine{};
  engine.init(display, transform, "./textures/brick.jpg");

  engine.createMesh(vertices, indices);

  engine.entities = {
    {.meshIndex=0,.pos={0.0f,0.0f,0.0f}},
  };

  while (engine.isRunning) {
    engine.processInput();
    engine.drawFrame();
  }

  engine.destroy();
}
