cmake_minimum_required(VERSION 3.7)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NAME VulkanRenderer)

project(${NAME})

find_package(Vulkan 1.3 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

add_subdirectory(lib/vk-bootstrap)

file(COPY ./shaders/ DESTINATION ./shaders/)
file(COPY ./textures/ DESTINATION ./textures/)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} vk-bootstrap::vk-bootstrap glm::glm Vulkan::Vulkan SDL2::SDL2)
