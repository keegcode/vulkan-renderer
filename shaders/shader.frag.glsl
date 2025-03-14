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
  vec3 camera;
}
proj;

layout (set = 2, binding = 0) uniform SceneLights {
  uint pointLights;
  uint spotLights;
} sceneLights;

layout(set = 2, binding = 1) uniform DirectionalLight {
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
}
directionalLight;

layout(set = 2, binding = 2) uniform PointLight {
  vec3 position;
  float constant;
  vec3 ambient;
  float linear;
  vec3 diffuse;
  float quadratic;
  vec3 specular;
}
pointLights[8];

layout(set = 2, binding = 3) uniform SpotLight {
  vec3 direction;
  float constant;
  vec3 position;
  float linear;
  vec3 ambient;
  float quadratic;
  vec3 diffuse;
  float cutOff;
  vec3 specular;
  float outerCutOff;
}
spotLights[8];

layout(set = 3, binding = 0) uniform Material {
  vec3 specular;
  float shininess;
  bool solid;
  bool light;
}
material;

vec3 calcDirectionLight(vec3 normal, vec3 viewDir) {
  vec3 lightDir = normalize(directionalLight.direction);

  float diff = max(dot(-lightDir, normal), 0.0);
  vec3 diffuse = directionalLight.diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(lightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular = directionalLight.specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient = directionalLight.ambient * vec3(texture(diffuseMap, inTexCoord));

  return (specular + ambient + diffuse);
}

vec3 calcPointLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = pointLights[idx].position;
  vec3 lightDir = normalize(lightPos - fragPos);

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse = pointLights[idx].diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(-lightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular = pointLights[idx].specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient = pointLights[idx].ambient * vec3(texture(diffuseMap, inTexCoord));

  float distance = length(lightPos - fragPos);
  float attenuation = 1.0 / (pointLights[idx].constant + pointLights[idx].linear * distance + pointLights[idx].quadratic * pow(distance, 2));

  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  return (specular + ambient + diffuse);
}

vec3 calcSpotLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = spotLights[idx].position;
  vec3 lightDir = normalize(spotLights[idx].direction);

  vec3 fragLightDir = normalize(lightPos - fragPos);

  float theta = dot(fragLightDir, -lightDir);
  vec3 ambient = spotLights[idx].ambient * vec3(texture(diffuseMap, inTexCoord));

  if (theta < spotLights[idx].outerCutOff) {
    return ambient;
  }

  float epsilon = spotLights[idx].cutOff - spotLights[idx].outerCutOff;
  float intensity = clamp((theta - spotLights[idx].outerCutOff) / epsilon, 0.0, 1.0);

  float diff = max(dot(fragLightDir, normal), 0.0);
  vec3 diffuse = spotLights[idx].diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(-fragLightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular = spotLights[idx].specular * spec * vec3(texture(specularMap, inTexCoord));

  float distance = length(lightPos - fragPos);
  float attenuation = 1.0 / (spotLights[idx].constant + spotLights[idx].linear * distance + spotLights[idx].quadratic * pow(distance, 2));
  
  diffuse *= intensity;
  specular *= intensity;
  
  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  return (specular + ambient + diffuse);
}

void main() {
  if (material.light) {
    outColor = vec4(inColor, 1.0);
    return;
  }

  vec3 normal = normalize(inNormals);
  vec3 viewDir = normalize(proj.camera - inFragPos);

  vec3 shadow = vec3(0.0);
  shadow += calcDirectionLight(normal, viewDir);

  for (uint i = 0; i < sceneLights.pointLights; i++) {
    shadow += calcPointLight(i, normal, inFragPos, viewDir);
  }

  for (uint i = 0; i < sceneLights.spotLights; i++) {
    shadow += calcSpotLight(i, normal, inFragPos, viewDir);
  }
  
  vec3 color = material.solid ? inColor : vec3(texture(tex0, inTexCoord));

  outColor = vec4(color * shadow, 1.0);
}


