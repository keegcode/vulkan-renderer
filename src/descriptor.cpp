#include "descriptor.hpp"

void Descriptor::destroy(const VmaAllocator& allocator) {
  buffer.destroy(allocator);
}
