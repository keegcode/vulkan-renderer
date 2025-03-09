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

layout(set = 4, binding = 0) uniform Object {
  mat4 matrix;
  vec3 color;
}
object;

void main() {
  vec3 pos =
      (proj.view * object.matrix * proj.model * vec4(inPosition, 1.0)).xyz;
  vec3 normal = normalize((proj.view * object.matrix * proj.model *
                           vec4(inNormals, 0.0)))
                    .xyz;

  outTexCoord = inTexCoord;
  outColor = object.color;
  outNormals = normal;
  outFragPos = pos;

  gl_Position = proj.perspective * vec4(pos, 1.0);
}
