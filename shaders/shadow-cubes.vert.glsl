#version 450

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 outLightToFrag;

layout(scalar, push_constant) uniform PushConstant {
  uint entityId;
  vec3 lightPos;
}
pushConstant;

layout(scalar, set = 0, binding = 0) uniform Transform {
  mat4 model;
  mat4 projection[6];
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
  vec4 pos = (entities.data[pushConstant.entityId].matrix * transform.model *
              vec4(inPosition, 1.0));
  outLightToFrag = vec3(pos) - pushConstant.lightPos;
  gl_Position = transform.projection[gl_ViewIndex] * pos;
}
