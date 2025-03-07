#include "scene.hpp"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vk_mem_alloc.h"

#include "engine.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

int32_t main() {
  Display display{};
  display.init();

  Engine engine{};
  engine.init(display);

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective = glm::perspective(
      glm::radians(60.0f), display.displayMode.w / (float)display.displayMode.h,
      0.1f, 100.0f);

  perspective[1][1] *= -1;

  Projection proj{model, view, perspective};
  engine.projection = proj;

  engine.loadMesh("./assets/cube.obj");
  engine.loadMesh("./assets/suzanne.obj");

  engine.loadTexture("./textures/default.jpg");

  Material material{};
  material.solid = 1;
  material.ambient = glm::vec3{1.0, 1.0, 1.0};
  material.diffuse = glm::vec3{1.0, 1.0, 1.0};
  material.specular = glm::vec3{1.0, 1.0, 1.0};
  material.shininess = 0.5;

  engine.materials.push_back(material);

  Object obj1{};
  obj1.matrix = glm::scale(glm::translate(obj1.matrix, glm::vec3{0.0, 0.0, -10.0}), glm::vec3{2.5f});
  obj1.textureIdx = 1;
  obj1.meshIdx = 0;
  obj1.materialIdx = 0;

  engine.objects.push_back(obj1);

  Light light{glm::vec3{0.0}, glm::vec3{1.0}, 0.03};
  engine.setLight(light);

  float previousTicks = SDL_GetTicks();
  float deltaTime;

  while (engine.isRunning) {
    float currentTicks = SDL_GetTicks();
    deltaTime = currentTicks - previousTicks;
    previousTicks = currentTicks;

    engine.processInput(deltaTime);
    engine.drawFrame(deltaTime);

    SDL_Delay(1);
  }

  engine.destroy();
}
