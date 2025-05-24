#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec4 outPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;

layout(scalar, push_constant) uniform FrameData {
  vec3 camera;
  uint pointLights;
  uint spotLights;
}
frameData;

layout(scalar, set = 5, binding = 0) uniform Transform {
  mat4 model;
  mat4 view;
  mat4 projection;
}
transform;

layout(scalar, set = 3, binding = 0) uniform EntityBuffer {
  mat4 matrix;
}
entity;

void main() {
  vec4 pos = (entity.matrix * transform.model * vec4(inPosition, 1.0));

  vec3 normal = vec3(entity.matrix * transform.model * vec4(inNormal, 0.0));
  vec3 tangent = vec3(entity.matrix * transform.model * vec4(inTangent, 0.0));
  vec3 bitangent =
      vec3(entity.matrix * transform.model * vec4(inBitangent, 0.0));

  outTexCoord = inTexCoord;
  outColor = inColor;
  outPos = pos;
  outNormal = normal;
  outTangent = tangent;
  outBitangent = bitangent;

  gl_Position = transform.projection * vec4(vec3(transform.view * pos), 1.0);
}
