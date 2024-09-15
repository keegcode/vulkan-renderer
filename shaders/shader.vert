#version 450 

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;

layout(binding = 1) uniform Projection {
  mat4 world;
  mat4 view;
  mat4 proj;
} prjct;

void main() {
  gl_Position = prjct.proj * prjct.view * prjct.world * vec4(inPosition, 1.0);
  outColor = inColor;
  outTexCoord = inTexCoord;
}
