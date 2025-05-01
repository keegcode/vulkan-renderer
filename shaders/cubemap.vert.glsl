#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 outTexCoord;

layout(push_constant) uniform FrameData {
  mat4 matrix;
}
frameData;

void main() {
  vec4 pos = (frameData.matrix * vec4(inPosition, 1.0)).xyww;
  outTexCoord = inPosition;
  gl_Position = pos;
}
