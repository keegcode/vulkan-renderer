#version 450 

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormals;

layout(set = 1, binding = 0) uniform Projection {
  mat4 model;
  mat4 view;
  mat4 perspective;
} proj;

layout(set = 1, binding = 1) uniform Object {
  vec3 pos;
} object;

void main() {
  gl_Position = proj.perspective * ((proj.view * proj.model * vec4(inPosition, 1.0)) + vec4(object.pos, 0.0));
  outColor = inColor;
  outTexCoord = inTexCoord;
  outNormals = outNormals;
}
