#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;

layout(scalar, push_constant) uniform FrameData {
  mat4 lightSpaceMatrix;
  mat4 model;
  uint entityId;
}
frameData;

struct Entity {
   mat4 matrix;
};

layout(scalar, set = 0, binding = 0) readonly buffer Entities {
  Entity data[];
}
entities;

void main() {
  gl_Position = frameData.lightSpaceMatrix * entities.data[frameData.entityId].matrix * frameData.model *
                vec4(inPosition, 1.0);
}
