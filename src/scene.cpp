#include "scene.hpp"
#include "descriptor.hpp"

Object::Object() {}

Object::Object(const ObjectProperties& p, const Descriptor& d, const Buffer& b)
    : properties{p}, descriptor{d}, buffer{b} {}

void Object::destroy(const VmaAllocator& allocator) {
  descriptor.destroy(allocator);
}

Material::Material() {};

Material::Material(const MaterialProperties& p,
                   const Descriptor& d,
                   const Buffer& b)
    : properties{p}, descriptor{d}, buffer{b} {};

void Material::destroy(const VmaAllocator& allocator) {
  descriptor.destroy(allocator);
};

Light::Light() {}

Light::Light(const LightProperties& p, const Descriptor& d, const Buffer& b)
    : properties{p}, descriptor{d}, buffer{b} {}

void Light::destroy(const VmaAllocator& allocator) {
  descriptor.destroy(allocator);
}

Texture::Texture(const Image& i, const Descriptor& d, const vk::Sampler& s)
    : image{i}, descriptor{d}, sampler{s} {}

void Texture::destroy(const VmaAllocator& allocator, const vk::Device& device) {
  image.destroy(allocator, device);
  descriptor.destroy(allocator);
}

Projection::Projection() {}

Projection::Projection(const ProjectionProperties& p,
                       const Descriptor& d,
                       const Buffer& b)
    : properties{p}, descriptor{d}, buffer{b} {}
