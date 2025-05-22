#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;

layout(scalar, push_constant) uniform FrameData {
  mat4 lightSpaceMatrix;
  mat4 model;
  uint shadowMapX;
  uint shadowMapY;
}
frameData;

layout(scalar, set = 0, binding = 0) uniform EntityBuffer {
  mat4 matrix;
}
entity;

void main() {
  gl_Position = frameData.lightSpaceMatrix * entity.matrix * frameData.model *
                vec4(inPosition, 1.0);
}
