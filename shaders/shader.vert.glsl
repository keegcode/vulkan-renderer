#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outFragPos;
layout(location = 3) out vec3 outNormals;

layout(set = 1, binding = 0) uniform Projection {
  mat4 model;
  mat4 view;
  mat4 perspective;
}
proj;

layout(set = 4, binding = 0) uniform Entity {
  mat4 matrix;
  vec3 color;
}
entity;

void main() {
  vec4 pos =
      (entity.matrix * proj.model * vec4(inPosition, 1.0));

  vec4 normal =
      (entity.matrix * proj.model * vec4(inNormals, 0.0));

  outTexCoord = inTexCoord;
  outColor = entity.color;
  outNormals = vec3(normal);
  outFragPos = vec3(pos);

  gl_Position = proj.perspective * vec4(vec3(proj.view * pos), 1.0);
}
