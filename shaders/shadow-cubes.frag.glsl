#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inLightToFrag;

layout(location = 0) out float outColor;

layout(scalar, push_constant) uniform PushConstant {
  uint entityId;
  vec3 lightPos;
}
pushConstant;

void main() {
  outColor = dot(inLightToFrag, inLightToFrag);
}
