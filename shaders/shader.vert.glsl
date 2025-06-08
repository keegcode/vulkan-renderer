#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec4 outPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec4 outTangent;

layout(scalar, push_constant) uniform PushConstant {
  vec3 camera;
  uint pointLights;
  uint spotLights;
  uint materialId;
  uint entityId;
}
pushConstant;

layout(scalar, set = 0, binding = 0) uniform Transform {
  mat4 model;
  mat4 view;
  mat4 projection;
}
transform;

struct Entity {
  mat4 matrix;
  uint modelIdx;
};

layout(scalar, set = 1, binding = 0) readonly buffer Entities {
  Entity data[];
}
entities;

void main() {
  Entity entity = entities.data[pushConstant.entityId];

  vec4 pos = (entity.matrix * transform.model * vec4(inPosition, 1.0));

  vec3 normal = vec3(entity.matrix * transform.model * vec4(inNormal, 0.0));
  vec3 tangent =
      vec3(entity.matrix * transform.model * vec4(vec3(inTangent), 0.0));

  outTexCoord = inTexCoord;
  outColor = inColor;
  outPos = pos;
  outNormal = normal;
  outTangent = vec4(tangent, inTangent.w);

  gl_Position = transform.projection * vec4(vec3(transform.view * pos), 1.0);
}
