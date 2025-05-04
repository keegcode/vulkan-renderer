#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outPos;
layout(location = 3) out vec4 outLightPos;
layout(location = 4) out vec3 outNormals;

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
} transform;

layout(scalar, set = 2, binding = 0) uniform DirectionalLight {
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  mat4 lightSpaceMatrix;
}
directionalLight;

layout(scalar, set = 3, binding = 0) uniform EntityBuffer {
  mat4 matrix;
}
entity;

void main() {
  vec4 pos = (entity.matrix * transform.model * vec4(inPosition, 1.0));
  vec4 lightPos = (directionalLight.lightSpaceMatrix * entity.matrix * transform.model * vec4(inPosition, 1.0));
  vec4 normal = (entity.matrix * transform.model * vec4(inNormals, 0.0));

  outTexCoord = inTexCoord;
  outColor = inColor;
  outNormals = vec3(normal);
  outLightPos = lightPos;
  outPos = vec3(pos);

  gl_Position = transform.projection * vec4(vec3(transform.view * pos), 1.0);
}
