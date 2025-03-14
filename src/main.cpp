#include <glm/ext/vector_float3.hpp>
#include "scene.hpp"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vk_mem_alloc.h"

#include "engine.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

std::vector<glm::vec3> cubePositions = {
    glm::vec3( 0.0f,  0.0f,  0.0f), 
    glm::vec3( 2.0f,  5.0f, -15.0f), 
    glm::vec3(-1.5f, -2.2f, -2.5f),  
    glm::vec3(-3.8f, -2.0f, -12.3f),  
    glm::vec3( 2.4f, -0.4f, -3.5f),  
    glm::vec3(-1.7f,  3.0f, -7.5f),  
    glm::vec3( 1.3f, -2.0f, -2.5f),  
    glm::vec3( 1.5f,  2.0f, -2.5f), 
    glm::vec3( 1.5f,  0.2f, -1.5f), 
    glm::vec3(-1.3f,  1.0f, -1.5f)  
};

int32_t main() {
  Display display{};
  display.init();

  Engine engine{};
  EngineState state{};

  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 perspective{1.0f};

  perspective = glm::perspective(
      glm::radians(60.0f), display.width / (float)display.height,
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
  
  Entity light;
  light.meshIdx = 0;
  light.materialIdx = 0;
  light.textureIdx = 0;
  light.properties.color = glm::vec3{1.0};
    
  DirectionalLightProperties directionalLight{};
  directionalLight.direction = glm::vec3{0.0, -1.0, 0.0};
  directionalLight.ambient = glm::vec3{0.1};
  directionalLight.diffuse = glm::vec3{0.1};
  directionalLight.specular = glm::vec3{0.1};

  PointLightProperties pointLight{};
  pointLight.position = glm::vec3{0.0, 0.0, -5.0};
  pointLight.ambient = glm::vec3{0.08};
  pointLight.diffuse = glm::vec3{1.0};
  pointLight.specular = glm::vec3{0.7};
  pointLight.constant = 1.0;
  pointLight.linear = 0.02;
  pointLight.quadratic = 0.016;


  SpotLightProperties spotLight{};
  spotLight.position = glm::vec3{-2.0, 0.0, 0.0};
  spotLight.direction = glm::vec3{1.0, 0.0, 0.0};
  spotLight.cutOff = glm::cos(glm::radians(7.0f));
  spotLight.outerCutOff = glm::cos(glm::radians(13.0f));
  spotLight.ambient = glm::vec3{0.06};
  spotLight.diffuse = glm::vec3{0.8};
  spotLight.specular = glm::vec3{0.7};
  spotLight.constant = 1.0;
  spotLight.linear = 0.2;
  spotLight.quadratic = 0.16;

  state.directionalLight = directionalLight;
  light.properties.matrix = glm::scale(glm::translate(glm::mat4{1.0}, glm::vec3{0.0, 5.0, 0.0}), glm::vec3{0.2});
  state.entities.push_back(light);

  state.pointLights.push_back(pointLight);
  light.properties.matrix = glm::scale(glm::translate(glm::mat4{1.0}, pointLight.position), glm::vec3{0.2});
  state.entities.push_back(light);

  state.spotLights.push_back(spotLight);
  light.properties.matrix = glm::scale(glm::translate(glm::mat4{1.0}, spotLight.position), glm::vec3{0.2});
  state.entities.push_back(light);

  for(size_t i = 0; i < cubePositions.size(); i++) {
    const glm::vec3& pos = cubePositions[i];
    Entity entity{};
    
    entity.properties.matrix =
        glm::scale(glm::translate(glm::mat4{.0f}, glm::vec3{0.0, 0.0, -10.0}),
                   glm::vec3{.5f});
    entity.properties.color = glm::vec3{0.5};
    entity.textureIdx = 1;
    entity.meshIdx = 0;
    entity.materialIdx = 1;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    float angle = 20.0f * i;
    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
    model = glm::scale(model, glm::vec3{0.5f});

    entity.properties.matrix = model;

    state.entities.push_back(entity);
  }

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
