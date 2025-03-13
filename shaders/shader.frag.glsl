#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 0, binding = 1) uniform sampler2D diffuseMap;
layout(set = 0, binding = 2) uniform sampler2D specularMap;

layout(set = 1, binding = 0) uniform Projection {
  mat4 model;
  mat4 view;
  mat4 perspective;
}
proj;

layout(set = 2, binding = 0) uniform Light {
  vec3 pos;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
}
light;

layout(set = 3, binding = 0) uniform Material {
  vec3 specular;
  float shininess;
  bool solid;
  bool light;
}
material;

void main() {
  vec3 normal = normalize(inNormals);

  vec3 direction =
      normalize((proj.view * vec4(light.pos, 1.0)).xyz - inFragPos);
  float diff = max(dot(direction, normal), 0.0);
  vec3 diffuse = light.diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 view = normalize(-inFragPos);
  vec3 reflection = reflect(-direction, normal);

  float spec = pow(max(dot(view, reflection), 0.0), material.shininess);
  vec3 specular = light.specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient = light.ambient * vec3(texture(diffuseMap, inTexCoord));

  vec3 shadow = (specular + ambient + diffuse);

  if (material.light) {
    outColor = vec4(inColor, 1.0);
    return;
  }

  if (material.solid) {
    outColor = vec4(inColor * shadow, 1.0);
  } else {
    outColor = vec4(texture(tex0, inTexCoord).xyz * shadow, 1.0);
  }
}
