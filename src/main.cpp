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

  state.textures.push_back({"./textures/default.jpg", "./textures/default.jpg",
                            "./textures/default.jpg"});

  state.textures.push_back({"./textures/container2.png",
                            "./textures/container2.png",
                            "./textures/container2_specular.png"});

  MaterialProperties lightMaterial{};
  lightMaterial.light = true;
  lightMaterial.shininess = 1.0;

  MaterialProperties material{};
  material.solid = true;
  material.light = false;
  material.specular = glm::vec3{1.0};
  material.shininess = 256.0;

  state.materials.push_back(lightMaterial);
  state.materials.push_back(material);

  Entity entity1{};
  entity1.properties.matrix =
      glm::scale(glm::translate(glm::mat4{1.0f}, glm::vec3{0.0, 0.0, -10.0}),
                 glm::vec3{1.5f});
  entity1.properties.color = glm::vec3{0.5};
  entity1.textureIdx = 1;
  entity1.meshIdx = 0;
  entity1.materialIdx = 1;

  LightProperties light;
  light.ambient = glm::vec3{0.1};
  light.diffuse = glm::vec3{0.4};
  light.specular = glm::vec3{1.0};
  light.pos = glm::vec3{6.0, 4.0, -5.0};

  state.light = light;

  Entity lightEntity{};
  lightEntity.properties.matrix =
      glm::scale(glm::translate(glm::mat4{1.0f}, light.pos), glm::vec3{0.5f});
  lightEntity.properties.color = glm::vec3{1.0};
  lightEntity.textureIdx = 0;
  lightEntity.meshIdx = 0;
  lightEntity.materialIdx = 0;

  state.entities.push_back(lightEntity);
  state.entities.push_back(entity1);

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
