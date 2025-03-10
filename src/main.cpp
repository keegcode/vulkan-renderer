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
  EngineState state{};

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective = glm::perspective(
      glm::radians(60.0f), display.displayMode.w / (float)display.displayMode.h,
      0.1f, 100.0f);

  perspective[1][1] *= -1;

  ProjectionProperties proj{model, view, perspective};
  state.projection = proj;

  state.meshes.push_back("./assets/cube.obj");
  state.meshes.push_back("./assets/suzanne.obj");

  state.textures.push_back("./textures/default.jpg");

  MaterialProperties lightMaterial{};
  lightMaterial.solid = 1;
  lightMaterial.ambient = glm::vec3{1.0};
  lightMaterial.diffuse = glm::vec3{1.0};
  lightMaterial.specular = glm::vec3{1.0};
  lightMaterial.brightness = 100.0;

  MaterialProperties material{};
  material.solid = 0;
  material.ambient = glm::vec3{0.5};
  material.diffuse = glm::vec3{0.5};
  material.specular = glm::vec3{0.5};
  material.brightness = 1.0;

  state.materials.push_back(lightMaterial);
  state.materials.push_back(material);

  Entity entity1{};
  entity1.properties.scale = glm::scale(glm::mat4{1.0f}, glm::vec3{2.5f});
  entity1.properties.translation =
      glm::translate(glm::mat4{1.0f}, glm::vec3{0.0, 0.0, -10.0});
  entity1.properties.color = glm::vec3{0.5};
  entity1.textureIdx = 0;
  entity1.meshIdx = 1;
  entity1.materialIdx = 1;

  LightProperties light;
  light.color = glm::vec3{1.0};
  light.pos = glm::vec3{0.0};
  light.ambient = 0.03;

  state.light = light;

  Entity lightEntity{};
  lightEntity.properties.scale = glm::scale(glm::mat4{1.0f}, glm::vec3{0.5f});
  lightEntity.properties.translation =
      glm::translate(glm::mat4{1.0f}, light.pos);
  lightEntity.properties.color = glm::vec3{1.0};
  lightEntity.textureIdx = 0;
  lightEntity.meshIdx = 0;
  lightEntity.materialIdx = 0;

  state.entities.push_back(entity1);
  state.entities.push_back(lightEntity);

  engine.init(display, state);

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
