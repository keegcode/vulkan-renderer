cmake_minimum_required(VERSION 3.10)

project(VulkanRenderer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan 1.3 REQUIRED)          
find_package(SDL3 REQUIRED)          
find_package(glm REQUIRED)         
find_package(assimp REQUIRED) 

file(COPY "${PROJECT_SOURCE_DIR}/shaders/"   DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(COPY "${PROJECT_SOURCE_DIR}/textures/"  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/textures/")
file(COPY "${PROJECT_SOURCE_DIR}/assets/"    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets/")

add_executable(${PROJECT_NAME} 
  src/main.cpp 
  src/engine.cpp 
  src/engine.hpp 
  src/image.cpp
  src/image.hpp
  src/buffer.cpp
  src/buffer.hpp
  src/display.cpp
  src/display.hpp
  src/shader.cpp
  src/shader.hpp
  src/scene.cpp
  src/scene.hpp
  src/descriptor.cpp
  src/descriptor.hpp
  src/utils.cpp
  src/utils.hpp
  src/pipeline.cpp
  src/pipeline.hpp
  src/stb_image.h
  src/vk_mem_alloc.h
  src/VkBootstrap.cpp
  src/VkBootstrap.h
  src/VkBootstrapDispatch.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC 
        assimp
        glm::glm
        Vulkan::Vulkan
        SDL3::SDL3
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Wall)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
