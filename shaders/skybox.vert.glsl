#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 outTexCoord;

layout(scalar, push_constant) uniform PushConstant {
  mat4 matrix;
}
pushConstant;

void main() {
  vec4 pos = (pushConstant.matrix * vec4(inPosition, 1.0)).xyww;
  outTexCoord = inPosition;
  gl_Position = pos;
}
