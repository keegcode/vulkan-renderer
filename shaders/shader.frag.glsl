#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inPos;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform FrameData {
  mat4 model;
  mat4 view;
  mat4 perspective;
  vec3 camera;
  uint pointLights;
  uint spotLights;
}
frameData;

layout(set = 0, binding = 0) uniform sampler2D diffuseMap;
layout(set = 0, binding = 1) uniform sampler2D specularMap;

layout(set = 1, binding = 0) uniform Material {
  vec3 specular;
  vec3 emissive;
  vec3 color;
  float shininess;
}
material;

layout(set = 2, binding = 0) uniform DirectionalLight {
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
}
directionalLight;

layout(set = 2, binding = 1) uniform PointLight {
  vec3 position;
  float constant;
  vec3 ambient;
  float linear;
  vec3 diffuse;
  float quadratic;
  vec3 specular;
}
pointLights[8];

layout(set = 2, binding = 2) uniform SpotLight {
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

vec3 calcDirectionLight(vec3 normal, vec3 viewDir) {
  vec3 lightDir = normalize(directionalLight.direction);

  float diff = max(dot(-lightDir, normal), 0.0);
  vec3 diffuse =
      directionalLight.diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(lightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular =
      directionalLight.specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient =
      directionalLight.ambient * vec3(texture(diffuseMap, inTexCoord));

  return (specular + ambient + diffuse);
}

vec3 calcPointLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = pointLights[idx].position;
  vec3 lightDir = normalize(lightPos - fragPos);

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse =
      pointLights[idx].diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(-lightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular =
      pointLights[idx].specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient =
      pointLights[idx].ambient * vec3(texture(diffuseMap, inTexCoord));

  float distance = length(lightPos - fragPos);
  float attenuation =
      1.0 / (pointLights[idx].constant + pointLights[idx].linear * distance +
             pointLights[idx].quadratic * pow(distance, 2));

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
  vec3 ambient =
      spotLights[idx].ambient * vec3(texture(diffuseMap, inTexCoord));

  if (theta < spotLights[idx].outerCutOff) {
    return ambient;
  }

  float epsilon = spotLights[idx].cutOff - spotLights[idx].outerCutOff;
  float intensity =
      clamp((theta - spotLights[idx].outerCutOff) / epsilon, 0.0, 1.0);

  float diff = max(dot(fragLightDir, normal), 0.0);
  vec3 diffuse =
      spotLights[idx].diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 reflection = reflect(-fragLightDir, normal);

  float spec = pow(max(dot(viewDir, reflection), 0.0), material.shininess);
  vec3 specular =
      spotLights[idx].specular * spec * vec3(texture(specularMap, inTexCoord));

  float distance = length(lightPos - fragPos);
  float attenuation =
      1.0 / (spotLights[idx].constant + spotLights[idx].linear * distance +
             spotLights[idx].quadratic * pow(distance, 2));

  diffuse *= intensity;
  specular *= intensity;

  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  return (specular + ambient + diffuse);
}

void main() {
  vec3 normal = normalize(inNormals);
  vec3 viewDir = normalize(frameData.camera - inPos);

  vec3 shadow = vec3(0.0);
  shadow += calcDirectionLight(normal, viewDir);

  for (uint i = 0; i < frameData.pointLights; i++) {
    shadow += calcPointLight(i, normal, inPos, viewDir);
  }

  for (uint i = 0; i < frameData.spotLights; i++) {
    shadow += calcSpotLight(i, normal, inPos, viewDir);
  }

  vec4 color = texture(diffuseMap, inTexCoord);

  if (color.a < 0.01) {
    discard;
  }

  outColor =
      vec4(inColor * material.color * vec3(color) * shadow, 1.0);
}
