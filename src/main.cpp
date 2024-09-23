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
  glm::mat4 proj{1.0f};
  
  model = glm::rotate(model, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  proj = glm::perspective(glm::radians(45.0f), display.displayMode.w / (float) display.displayMode.h, 0.1f, 10.0f);

  proj[1][1] *= -1;

  Transform transform{model, view, proj};

  VkEngine engine{};
  engine.init(display, transform, "./assets/stalker.obj", "./textures/stalker.png");

  engine.entities = {
    {.meshIndex=0,.pos={0.0f,0.0f,0.0f}},
  };

  while (engine.isRunning) {
    engine.processInput();
    engine.drawFrame();
  }

  engine.destroy();
}
