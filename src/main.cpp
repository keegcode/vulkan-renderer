#include <glm/ext/vector_float3.hpp>

#define VMA_IMPLEMENTATION

#define VMA_STATIC_VULKAN_FUNCTIONS 0;
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1;

#include "engine.hpp"
#include "vk_mem_alloc.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

int32_t main() {
  uint32_t shadowSize = 512;

  Display display{};
  GPU gpu{display, shadowSize};

  EngineConfig config{};

  config.shadowSize = shadowSize;

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective = glm::perspective(
      glm::radians(70.0f), static_cast<float>(display.width) / static_cast<float>(display.height), 0.5f, 500.0f);

  perspective[1][1] *= -1;

  Transform transform{model, view, perspective};
  config.transform = transform;

  config.assets.push_back("./assets/Sponza/glTF/Sponza.gltf");
  config.assets.push_back("./assets/DamagedHelmet/glTF/DamagedHelmet.gltf");
  config.assets.push_back(
      "./assets/GlassBrokenWindow/glTF/GlassBrokenWindow.gltf");

  config.directionalLight.direction =
      glm::normalize(glm::vec3{0.0f, -150.0f, 0.0f});
  config.directionalLight.ambient = glm::vec3{0.2f};
  config.directionalLight.diffuse = glm::vec3{0.8f};
  config.directionalLight.specular = glm::vec3{0.4f};

  config.directionalLight.lightSpaceMatrix =
      glm::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 10.0f, 400.0f);
  config.directionalLight.lightSpaceMatrix[1][1] *= -1;
  config.directionalLight.lightSpaceMatrix *= glm::lookAt(
      glm::vec3{0.1f, 350.0f, 0.1f}, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});

  PointLight pointLight{};
  pointLight.position = glm::vec3{0.0f, 10.0f, -50.0f};
  pointLight.ambient = glm::vec3{0.1f};
  pointLight.diffuse = glm::vec3{0.5f};
  pointLight.specular = glm::vec3{0.4f};
  pointLight.constant = 1.0f;
  pointLight.linear = 0.0013f;
  pointLight.lightSpaceMatrix = glm::perspective(
      glm::radians(90.0f), 1.0f, 1.0f, 200.0f);
  pointLight.lightSpaceMatrix[1][1] *= -1;
  pointLight.lightSpaceMatrix *= glm::lookAt(
      pointLight.position, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
  //config.pointLights.push_back(pointLight);

  SpotLight spotLight{};
  spotLight.position = glm::vec3{0.0f, 100.0f, 5.0f};
  spotLight.direction = glm::vec3{0.0f, -1.0f, 0.0f};
  spotLight.cutOff = glm::cos(glm::radians(10.0f));
  spotLight.outerCutOff = glm::cos(glm::radians(30.0f));
  spotLight.ambient = glm::vec3{0.1f};
  spotLight.diffuse = glm::vec3{0.5f};
  spotLight.specular = glm::vec3{0.4f};
  spotLight.constant = 1.0f;
  spotLight.linear = 0.0013f;
  spotLight.lightSpaceMatrix = glm::perspective(
      glm::radians(90.0f), 1.0f, 1.0f, 200.0f);
  spotLight.lightSpaceMatrix[1][1] *= -1;
  spotLight.lightSpaceMatrix *= glm::lookAt(spotLight.position, spotLight.direction, glm::vec3{0.0f, 1.0f, 0.0f});
  //config.spotLights.push_back(spotLight);

  Entity sponza{};
  sponza.matrix = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 0.0f, 0.0f}) *
                  glm::rotate(glm::mat4{1.0f}, glm::radians(90.0f),
                              glm::vec3{0.0f, 1.0f, 0.0f}) *
                  glm::scale(glm::mat4{1.0f}, glm::vec3{0.1f});
  sponza.assetIdx = 1;

  Entity helmet{};
  helmet.matrix =
      glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 20.0f, -25.0f}) *
      glm::rotate(glm::mat4{1.0f}, glm::radians(90.0f),
                  glm::vec3{1.0f, 0.0f, 0.0f}) *
      glm::scale(glm::mat4{1.0f}, glm::vec3{4.0f});
  helmet.assetIdx = 2;

  Entity window{};
  window.matrix =
      glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 15.0f, -5.0f}) *
      glm::scale(glm::mat4{1.0f}, glm::vec3{20.0f});
  window.assetIdx = 3;

  Entity window2{};
  window2.matrix =
      glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 15.0f, 25.0f}) *
      glm::scale(glm::mat4{1.0f}, glm::vec3{20.0f});
  window2.assetIdx = 3;

  config.entities.push_back(sponza);
  config.entities.push_back(helmet);
  config.entities.push_back(window);
  config.entities.push_back(window2);

  Engine engine{display, gpu, config};

  uint64_t previousTicks = SDL_GetTicks();

  uint64_t lastFrame = previousTicks;
  uint32_t frames = 0;

  uint64_t deltaTime = 0;

  while (engine.isRunning) {
    uint64_t currentTicks = SDL_GetTicks();

    if (currentTicks - lastFrame >= 1000) {
      std::string title =
          std::string{"Vulkan"} + " (" + std::to_string(frames) + " FPS)";
      SDL_SetWindowTitle(display.window, title.c_str());
      lastFrame = currentTicks;
      frames = 0;
    }

    deltaTime = currentTicks - previousTicks;
    previousTicks = currentTicks;

    engine.processInput(deltaTime);
    engine.drawFrame(deltaTime);

    SDL_Delay(1);
    frames += 1;
  }

  engine.destroy();
}
