#version 450 

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform Light {
  vec3 pos;
  vec3 color;
  float ambient;
} light;

layout(set = 0, binding = 0) uniform sampler2D tex0;

void main() {
  vec3 normal = normalize(inNormals);
  vec3 direction = normalize(light.pos - inFragPos);
  vec3 diffuse = max(dot(direction, normal), 0.0) * light.color;
  vec3 ambient = light.color * light.ambient;
  
  outColor = vec4((diffuse + ambient), 1.0) * texture(tex0, inTexCoord);
}
