cmake_minimum_required(VERSION 3.7)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(NAME VulkanRenderer)

project(${NAME})

find_package(Vulkan 1.3 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)

add_library(vma INTERFACE)
add_library(stb_image INTERFACE)

file(COPY ./shaders/ DESTINATION ./shaders/)
file(COPY ./textures/ DESTINATION ./textures/)
file(COPY ./assets/ DESTINATION ./assets/)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/src/*.hpp)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

add_subdirectory(./third_party/vk-bootstrap)

target_include_directories(vma INTERFACE ./third_party/vma)
target_include_directories(stb_image INTERFACE ./third_party/stb_image)

target_link_libraries(${PROJECT_NAME} PUBLIC assimp vma stb_image vk-bootstrap::vk-bootstrap glm::glm Vulkan::Vulkan SDL2::SDL2)
