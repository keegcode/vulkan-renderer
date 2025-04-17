#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 outTexCoord;

layout(push_constant) uniform FrameData {
  mat4 model;
  mat4 view;
  mat4 perspective;
  vec3 camera;
  uint pointLights;
  uint spotLights;
} frameData;

void main() {
  outTexCoord = inPosition;
  vec4 pos = frameData.perspective * mat4(mat3(frameData.view)) * vec4(inPosition, 1.0);
  gl_Position = pos.xyww;
}
