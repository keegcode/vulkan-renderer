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
} proj;

layout(set = 2, binding = 0) uniform Object {
  mat4 translation;
  mat4 rotation;
  mat4 scale;
  vec3 color;
} object;

void main() {
  vec3 pos = (object.translation * object.rotation * object.scale * proj.model * vec4(inPosition, 1.0)).xyz;
  vec3 normal = normalize((object.rotation * proj.model * vec4(inNormals, 1.0))).xyz;

  outTexCoord = inTexCoord;
  outColor = object.color;
  outNormals = normal;
  outFragPos = pos;

  gl_Position = proj.perspective * proj.view * vec4(pos, 1.0);
}
