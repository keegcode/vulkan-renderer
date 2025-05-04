#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inPos;
layout(location = 3) in vec4 inLightPos;
layout(location = 4) in vec3 inNormals;

layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform FrameData {
  vec3 camera;
  uint pointLights;
  uint spotLights;
}
frameData;

layout(set = 0, binding = 0) uniform sampler2D diffuseMap;
layout(set = 0, binding = 1) uniform sampler2D specularMap;

layout(scalar, set = 1, binding = 0) uniform Material {
  vec3 specular;
  float shininess;
  vec3 emissive;
  float alphaCutoff;
  vec3 color;
  float transmissionFactor;
  float roughness;
}
material;

layout(scalar, set = 2, binding = 0) uniform DirectionalLight {
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  mat4 lightSpaceMatrix;
}
directionalLight;

layout(scalar, set = 2, binding = 1) uniform PointLight {
  vec3 position;
  float constant;
  vec3 ambient;
  float linear;
  vec3 diffuse;
  float quadratic;
  vec3 specular;
}
pointLights[8];

layout(scalar, set = 2, binding = 2) uniform SpotLight {
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

layout(set = 4, binding = 0) uniform sampler2D shadowMap;
layout(set = 4, binding = 1) uniform samplerCube skybox;

float calcShadow() {
  vec4 pos = inLightPos / inLightPos.w;

  float closestDepth = texture(shadowMap, pos.st).r;
  float currentDepth = pos.z;

  float shadow = currentDepth > closestDepth ? 0.0 : 1.0;
  
  return closestDepth;
}

vec3 calcDirectionLight(vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightDir = normalize(-directionalLight.direction);
  vec3 halfDir = vec3(lightDir + viewDir);

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse =
      directionalLight.diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  float spec = pow(max(dot(normal, halfDir), 0.0), material.shininess);
  vec3 specular =
      directionalLight.specular * spec * vec3(texture(specularMap, inTexCoord));

  vec3 ambient =
      directionalLight.ambient * vec3(texture(diffuseMap, inTexCoord));

  float shadow = calcShadow();
  
  return (ambient + ((diffuse + specular) * shadow));
}

vec3 calcPointLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = pointLights[idx].position;
  vec3 lightDir = normalize(lightPos - fragPos);

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse =
      pointLights[idx].diffuse * diff * vec3(texture(diffuseMap, inTexCoord));

  vec3 halfDir = vec3(lightDir + viewDir);

  float spec = pow(max(dot(normal, halfDir), 0.0), material.shininess);
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

  vec3 halfDir = normalize(fragLightDir + viewDir);

  float spec = pow(max(dot(normal, halfDir), 0.0), material.shininess);
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

vec3 calcCubemapReflection(vec3 baseColor, vec3 viewDir, vec3 normal) {
  vec3 r = refract(-viewDir, normal, 0.7);
  vec4 color = texture(skybox, r);
  float dot = max(dot(normal, viewDir), 0.0);
  float reflectionStrength = mix(1.0, 0.0, pow(material.roughness, 2.0));
  float fresnel = pow(1.0 - dot, 5.0) * (1.0 - material.roughness);
  float reflectionFactor =
      reflectionStrength + fresnel * (1.0 - reflectionStrength);
  return mix(baseColor, vec3(color), reflectionFactor);
}

float linearizeDepth(float depth) {
	float zNear = 0.5f; 
	float zFar  = 500.0f;
	return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}

void main() {
  vec3 normal = normalize(inNormals);
  vec3 viewDir = normalize(frameData.camera - inPos);

  vec3 shadow = vec3(0.0);
  shadow += calcDirectionLight(normal, inPos, viewDir);

  for (uint i = 0; i < frameData.pointLights; i++) {
    shadow += calcPointLight(i, normal, inPos, viewDir);
  }

  for (uint i = 0; i < frameData.spotLights; i++) {
    shadow += calcSpotLight(i, normal, inPos, viewDir);
  }

  vec4 color = texture(diffuseMap, inTexCoord);

  if (color.a < material.alphaCutoff) {
    discard;
  }

  color.a *= 1.0 - material.transmissionFactor;
  color.xyz = calcCubemapReflection(color.xyz, viewDir, normal);

  float c = linearizeDepth(gl_FragCoord.z);
  vec4 fog = vec4(c, c, c, 1.0);

  outColor = mix(inColor * vec4(material.color, 1.0) * color * vec4(shadow, 1.0), fog, c * 0.03);
}
