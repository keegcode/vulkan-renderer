#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inPos;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform FrameData {
  vec3 camera;
  uint pointLights;
  uint spotLights;
  uint materialId;
  uint entityId;
}
frameData;

layout(scalar, set = 0, binding = 0) uniform Transform {
  mat4 model;
  mat4 view;
  mat4 projection;
}
transform;

struct Material {
  vec3 specular;
  float shininess;
  vec3 emissive;
  float alphaCutoff;
  vec3 color;
  float transmissionFactor;
  float roughness;
  uint normalTextureIdx;
  uint diffuseTextureIdx;
  uint specularTextureIdx;
  uint heightTextureIdx;
  uint alphaMode;
  uint cullMode;
};

layout(scalar, set = 1, binding = 1) readonly buffer Materials {
  Material data[];
}
materials;

layout(set = 2, binding = 0) uniform sampler2D textures[];
layout(set = 2, binding = 1) uniform sampler2DShadow shadowMaps[];
layout(set = 2, binding = 2) uniform samplerCube cubemaps[];

layout(scalar, set = 3, binding = 0) uniform Skylight {
  float intensity;
  uint cubemapTextureIdx;
}
skylight;

layout(scalar, set = 3, binding = 1) uniform DirectionalLight {
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  uint shadowMapIdx;
  mat4 lightSpaceMatrix;
  bool shadows;
}
directionalLight;

struct PointLight {
  vec3 position;
  float constant;
  vec3 ambient;
  float linear;
  vec3 diffuse;
  vec3 specular;
  uint shadowMapIdx;
  mat4 lightSpaceMatrix;
  bool shadows;
};

layout(scalar, set = 3, binding = 2) readonly buffer PointLights {
  PointLight data[];
}
pointLights;

struct SpotLight {
  vec3 direction;
  float constant;
  vec3 position;
  float linear;
  vec3 ambient;
  vec3 diffuse;
  float cutOff;
  vec3 specular;
  float outerCutOff;
  uint shadowMapIdx;
  mat4 lightSpaceMatrix;
  bool shadows;
};

layout(scalar, set = 3, binding = 3) readonly buffer SpotLights {
  SpotLight data[];
}
spotLights;

float calcShadow(vec4 inLightPos, uint shadowMapIdx, bool shadowsEnabled) {
  if (!shadowsEnabled) {
    return 1.0;
  }

  vec4 sampleLightPos = inLightPos / inLightPos.w;
  sampleLightPos.xy = sampleLightPos.xy * 0.5 + 0.5;

  return texture(shadowMaps[shadowMapIdx], sampleLightPos.xyz);
}

vec3 calcDirectionLight(vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightDir = normalize(-directionalLight.direction);
  vec3 halfDir = normalize(vec3(lightDir + viewDir));

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse =
      directionalLight.diffuse * diff *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  float spec = pow(max(dot(normal, halfDir), 0.0),
                   materials.data[frameData.materialId].shininess);
  vec3 specular =
      directionalLight.specular * spec *
      vec3(texture(
          textures[materials.data[frameData.materialId].specularTextureIdx],
          inTexCoord));

  vec3 ambient =
      directionalLight.ambient *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  float shadow =
      calcShadow(directionalLight.lightSpaceMatrix * inPos,
                 directionalLight.shadowMapIdx, directionalLight.shadows);

  return (ambient + ((diffuse + specular) * shadow));
}

vec3 calcPointLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = pointLights.data[idx].position;
  vec3 lightDir = normalize(lightPos - fragPos);

  float diff = max(dot(lightDir, normal), 0.0);
  vec3 diffuse =
      pointLights.data[idx].diffuse * diff *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  vec3 halfDir = normalize(vec3(lightDir + viewDir));

  float spec = pow(max(dot(normal, halfDir), 0.0),
                   materials.data[frameData.materialId].shininess);
  vec3 specular =
      pointLights.data[idx].specular * spec *
      vec3(texture(
          textures[materials.data[frameData.materialId].specularTextureIdx],
          inTexCoord));

  vec3 ambient =
      pointLights.data[idx].ambient *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  float distance = length(lightDir);

  float attenuation = 1.0 / (pointLights.data[idx].constant +
                             pointLights.data[idx].linear * distance);

  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  float shadow = calcShadow(pointLights.data[idx].lightSpaceMatrix * inPos,
                            pointLights.data[idx].shadowMapIdx,
                            pointLights.data[idx].shadows);

  return (ambient + ((specular + diffuse) * shadow));
}

vec3 calcSpotLight(uint idx, vec3 normal, vec3 fragPos, vec3 viewDir) {
  vec3 lightPos = spotLights.data[idx].position;
  vec3 lightVector = lightPos - fragPos;

  vec3 lightDir = normalize(spotLights.data[idx].direction);
  vec3 fragLightDir = normalize(lightVector);

  float theta = dot(fragLightDir, -lightDir);
  vec3 ambient =
      spotLights.data[idx].ambient *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  if (theta < spotLights.data[idx].outerCutOff) {
    return ambient;
  }

  float epsilon =
      spotLights.data[idx].cutOff - spotLights.data[idx].outerCutOff;
  float intensity =
      clamp((theta - spotLights.data[idx].outerCutOff) / epsilon, 0.0, 1.0);

  float diff = max(dot(fragLightDir, normal), 0.0);
  vec3 diffuse =
      spotLights.data[idx].diffuse * diff *
      vec3(texture(
          textures[materials.data[frameData.materialId].diffuseTextureIdx],
          inTexCoord));

  vec3 halfDir = normalize(fragLightDir + viewDir);

  float spec = pow(max(dot(normal, halfDir), 0.0),
                   materials.data[frameData.materialId].shininess);
  vec3 specular =
      spotLights.data[idx].specular * spec *
      vec3(texture(
          textures[materials.data[frameData.materialId].specularTextureIdx],
          inTexCoord));

  float distance = length(lightVector);
  float attenuation = 1.0 / (spotLights.data[idx].constant +
                             spotLights.data[idx].linear * distance);

  diffuse *= intensity;
  specular *= intensity;

  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  float shadow = calcShadow(spotLights.data[idx].lightSpaceMatrix * inPos,
                            spotLights.data[idx].shadowMapIdx,
                            spotLights.data[idx].shadows);

  return (ambient + ((specular + diffuse) * shadow));
}

float linearizeDepth(float depth) {
  float zNear = 0.5f;
  float zFar = 500.0f;
  return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}

vec3 calcSkyboxReflection(vec3 viewDir, vec3 normal, vec3 color) {
  vec3 r = refract(-viewDir, normal, 0.66);
  return mix(color, texture(cubemaps[skylight.cubemapTextureIdx], r).rgb,
             (1.0 - materials.data[frameData.materialId].roughness));
}

void main() {
  vec3 fragPos = vec3(inPos);
  vec3 viewDir = normalize(frameData.camera - fragPos);
  vec3 normal = normalize(inNormal);

  if (materials.data[frameData.materialId].normalTextureIdx != 1) {
    vec3 tangent = normalize(inTangent.xyz);
    tangent = (tangent - dot(tangent, normal) * normal) * inTangent.w;
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    normal =
        texture(textures[materials.data[frameData.materialId].normalTextureIdx],
                inTexCoord)
            .rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(TBN * normal);
  }

  vec3 shadow = vec3(0.0);
  shadow += calcDirectionLight(normal, fragPos, viewDir);

  for (uint i = 0; i < frameData.pointLights; i++) {
    shadow += calcPointLight(i, normal, fragPos, viewDir);
  }

  for (uint i = 0; i < frameData.spotLights; i++) {
    shadow += calcSpotLight(i, normal, fragPos, viewDir);
  }

  vec4 color =
      texture(textures[materials.data[frameData.materialId].diffuseTextureIdx],
              inTexCoord);

  if (color.a < materials.data[frameData.materialId].alphaCutoff) {
    discard;
  }

  color.a *= 1.0 - materials.data[frameData.materialId].transmissionFactor;

  float c = linearizeDepth(gl_FragCoord.z);
  vec4 fog = vec4(c, c, c, 1.0);

  color.xyz = calcSkyboxReflection(viewDir, normal, color.xyz);
  color = inColor * vec4(materials.data[frameData.materialId].color, 1.0) *
          color * vec4(shadow, 1.0);

  outColor = mix(color, fog, c * 0.03);
}
