#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform FrameData {
  mat4 viewProjection;
  mat4 model;
}
frameData;

layout(set = 0, binding = 0) uniform EntityBuffer {
  mat4 matrix;
}
entity;

void main() {
  gl_Position = frameData.viewProjection * entity.matrix * frameData.model * vec4(inPosition, 1.0);
}
