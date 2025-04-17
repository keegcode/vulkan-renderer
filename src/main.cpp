#include <glm/ext/vector_float3.hpp>
#include <utility>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "engine.hpp"
#include "vk_mem_alloc.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

int32_t main() {
  Display display{};
  display.init();

  GPU gpu{display};

  Engine engine{display, gpu};
  EngineConfig config{};

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective =
      glm::perspective(glm::radians(60.0f),
                       display.width / (float)display.height, 0.1f, 500.0f);

  perspective[1][1] *= -1;

  Projection proj{model, view, perspective};
  config.projection = proj;

  config.assets.push_back("./assets/Sponza/glTF/Sponza.gltf");
  config.assets.push_back("./assets/DamagedHelmet/glTF/DamagedHelmet.gltf");

  config.directionalLight.direction = glm::vec3{1.0, -1.0, 0.0};
  config.directionalLight.position = glm::vec3{0.0, 100.0, 0.0};
  config.directionalLight.ambient = glm::vec3{0.1};
  config.directionalLight.diffuse = glm::vec3{0.01};
  config.directionalLight.specular = glm::vec3{0.4};

  PointLight pointLight{};
  pointLight.position = glm::vec3{0.0, 5.0, 100.0};
  pointLight.ambient = glm::vec3{0.1};
  pointLight.diffuse = glm::vec3{0.8};
  pointLight.specular = glm::vec3{0.7};
  pointLight.constant = 1.0;
  pointLight.linear = 0.002;
  pointLight.quadratic = 0.00032;
  config.pointLights.push_back(pointLight);

  SpotLight spotLight{};
  spotLight.position = glm::vec3{0.0, 100.0, 5.0f};
  spotLight.direction = glm::vec3{0.0, -1.0, 0.0};
  spotLight.cutOff = glm::cos(glm::radians(10.0f));
  spotLight.outerCutOff = glm::cos(glm::radians(30.0f));
  spotLight.ambient = glm::vec3{0.1};
  spotLight.diffuse = glm::vec3{0.8};
  spotLight.specular = glm::vec3{0.7};
  spotLight.constant = 1.0;
  spotLight.linear = 0.0002;
  spotLight.quadratic = 0.000016;
  config.spotLights.push_back(spotLight);

  Entity sponza{};
  sponza.matrix = glm::translate(glm::rotate(glm::scale(glm::mat4{1.0f}, glm::vec3{0.1f}), glm::radians(90.0f), glm::vec3{0.0, 1.0, 0.0}), glm::vec3{0.0, 0.0f, 0.0f});
  sponza.assetIdx = 1;

  Entity helmet{};
  helmet.matrix = glm::translate(glm::rotate(glm::scale(glm::mat4{1.0f}, glm::vec3{4.0f}), glm::radians(90.0f), glm::vec3{1.0, 0.0, 0.0}), glm::vec3{0.0, -5.0f, -5.0f});
  helmet.assetIdx = 2;

  config.entities.push_back(sponza);
  config.entities.push_back(helmet);

  engine.init(config);

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
