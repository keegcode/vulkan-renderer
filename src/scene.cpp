#include "scene.hpp"

Texture::Texture(const Image& i) : image{i} {}

void Texture::destroy(const VmaAllocator& allocator, const vk::Device& device) {
  image.destroy(allocator, device);
}
