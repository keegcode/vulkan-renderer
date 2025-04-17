#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outPos;
layout(location = 3) out vec3 outNormals;

layout(push_constant) uniform FrameData {
  mat4 model;
  mat4 view;
  mat4 perspective;
  vec3 camera;
  uint pointLights;
  uint spotLights;
}
frameData;

layout(set = 3, binding = 0) uniform EntityBuffer {
  mat4 matrix;
}
entity;

void main() {
  vec4 pos = (entity.matrix * frameData.model * vec4(inPosition, 1.0));
  vec4 normal = (entity.matrix * frameData.model * vec4(inNormals, 0.0));

  outTexCoord = inTexCoord;
  outColor = inColor;
  outNormals = vec3(normal);
  outPos = vec3(pos);

  gl_Position = frameData.perspective * vec4(vec3(frameData.view * pos), 1.0);
}
