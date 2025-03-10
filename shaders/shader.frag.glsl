#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex0;

layout(set = 1, binding = 0) uniform Projection {
  mat4 model;
  mat4 view;
  mat4 perspective;
}
proj;

layout(set = 2, binding = 0) uniform Light {
  vec3 pos;
  vec3 color;
  float ambient;
}
light;

layout(set = 3, binding = 0) uniform Material {
  vec3 specular;
  vec3 ambient;
  vec3 diffuse;
  float brightness;
  uint solid;
}
material;

void main() {
  vec3 normal = normalize(inNormals);

  vec3 direction =
      normalize((proj.view * vec4(light.pos, 1.0)).xyz - inFragPos);
  float diffuse = max(dot(direction, normal), 0.0);

  vec3 view = normalize(-inFragPos);
  vec3 reflection = reflect(-direction, normal);

  float specular = pow(max(dot(view, reflection), 0.0), 32) * 0.5;

  outColor =
      vec4(((material.solid == 1 ? inColor : texture(tex0, inTexCoord).xyz) *
            light.color * (diffuse + light.ambient + specular)) *
               material.brightness,
           1.0);
}
