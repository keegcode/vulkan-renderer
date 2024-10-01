#include "scene.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#include "vk-engine.hpp"

int main() {
  Display display{};
  display.init();

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};
  
  model = glm::rotate(model, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  perspective = glm::perspective(glm::radians(80.0f), display.displayMode.w / (float) display.displayMode.h, 0.1f, 10.0f);

  perspective[1][1] *= -1;

  Projection proj{model, view, perspective};

  VkEngine engine{};

  engine.setProjection(proj);
  
  engine.init(display);
  
  engine.loadMesh("./assets/cube.obj");
  engine.loadTexture("./textures/brick.jpg");
  engine.loadTexture("./textures/box.jpg");

  engine.addObject(Object{glm::vec3{0.0,0.0,0.0},0,0});
  engine.addObject(Object{glm::vec3{0.0,5.0,0.0},1,0});

  while (engine.isRunning) {
    engine.processInput();
    engine.drawFrame();
  }

  engine.destroy();
}
