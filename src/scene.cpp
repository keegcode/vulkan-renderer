#include "scene.hpp"
#include "descriptor.hpp"

Texture::Texture(const Image& i)
    : image{i} {}

void Texture::destroy(const VmaAllocator& allocator, const vk::Device& device) {
  image.destroy(allocator, device);
  descriptor.destroy(allocator);
}

Projection::Projection() {}

Projection::Projection(const ProjectionProperties& p,
                       const Descriptor& d,
                       const Buffer& b)
    : properties{p}, descriptor{d}, buffer{b} {}
