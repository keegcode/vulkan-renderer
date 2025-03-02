#include "scene.hpp"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vk_mem_alloc.h"

#include "engine.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

int main() {
  Display display{};
  display.init();

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective = glm::perspective(
      glm::radians(60.0f), display.displayMode.w / (float)display.displayMode.h,
      0.1f, 100.0f);

  perspective[1][1] *= -1;

  ProjectionProperties proj{model, view, perspective};
  Engine engine{};

  engine.init(display);

  engine.setProjection(proj);

  engine.loadMesh("./assets/cube.obj");
  engine.loadMesh("./assets/suzanne.obj");

  engine.loadTexture("./textures/default.jpg");

  MaterialProperties material{};
  material.solid = 0;
  material.ambient = glm::vec3{1.0, 1.0, 1.0};
  material.diffuse = glm::vec3{1.0, 1.0, 1.0};
  material.specular = glm::vec3{1.0, 1.0, 1.0};
  material.shininess = 0.5;

  engine.addMaterial(material);

  ObjectProperties obj1{};
  obj1.translation =
      glm::translate(obj1.translation, glm::vec3{0.0, 0.0, -10.0});
  obj1.scale = glm::scale(obj1.scale, glm::vec3{2.5});

  engine.addObject(obj1, 1, 0, 0);

  LightProperties light{glm::vec3{0.0}, glm::vec3{1.0}, 0.03};

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
