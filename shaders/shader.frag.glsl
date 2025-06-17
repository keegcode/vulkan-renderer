#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define MAX_SHININESS 2048.0
#define PI 3.14159265359
#define DIFFUSE_INTENSITY 1.0 
#define SPECULAR_INTENSITY 1.0 
#define FRESNEL_0 0.05
#define EXPOSURE 0.6

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inPos;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform PushConstant {
  vec3 camera;
  uint pointLights;
  uint spotLights;
  uint materialId;
  uint entityId;
}
pushConstant;

layout(scalar, set = 0, binding = 0) uniform Transform {
  mat4 model;
  mat4 view;
  mat4 projection;
}
transform;

struct Material {
  vec3 color;
  vec3 emissive;
  float alphaCutoff;
  float transmissionFactor;
  float roughness;
  float metallic;
  uint normalTextureIdx;
  uint diffuseTextureIdx;
  uint metallicRoughnessTextureIdx;
  uint emissiveTextureIdx;
  uint cullMode;
  uint alphaMode;
};

layout(scalar, set = 1, binding = 1) readonly buffer Materials {
  Material data[];
}
materials;

layout(set = 2, binding = 0) uniform sampler2D textures[];
layout(set = 2, binding = 1) uniform sampler2DShadow shadowMaps[];
layout(set = 2, binding = 2) uniform samplerCube cubemaps[];
layout(set = 2, binding = 3) uniform samplerCubeShadow shadowCubes[];

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
  float shadowFactor;
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
  float shadowFactor;
  float farPlane;
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
  float shadowFactor;
};

layout(scalar, set = 3, binding = 3) readonly buffer SpotLights {
  SpotLight data[];
}
spotLights;

float calcPointShadow(vec3 fragPos, uint pointLightIdx, vec3 normal) {
  PointLight pointLight = pointLights.data[pointLightIdx];

  vec3 lightToFrag = fragPos - pointLight.position;
  float currentDepth = dot(lightToFrag, lightToFrag);

  float bias = 0.95;

  return texture(shadowCubes[pointLight.shadowMapIdx],
                 vec4(lightToFrag, currentDepth * bias));
}

float calcShadow(vec4 inLightPos, uint shadowMapIdx) {
  vec4 sampleLightPos = inLightPos / inLightPos.w;
  sampleLightPos.xy = sampleLightPos.xy * 0.5 + 0.5;

  return texture(shadowMaps[shadowMapIdx], sampleLightPos.xyz);
}

float calcFresnelFactor(vec3 fragToCameraDir, vec3 normal) {
  float fresnelDot = 1.0 - max(dot(fragToCameraDir, normal), 0.0);
  return FRESNEL_0 + ((1.0 - FRESNEL_0) * pow(fresnelDot, 5.0));
}

struct SurfaceColor {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float specularFactor;
  float metallicFactor;
};

SurfaceColor calcSurfaceColor(vec3 fragToLightDir,
                              vec3 halfDir,
                              vec3 normal,
                              vec3 ambientColor,
                              vec3 diffuseColor,
                              vec3 specularColor) {
  Material material = materials.data[pushConstant.materialId];

  float sampledRoughness =
      texture(textures[material.metallicRoughnessTextureIdx], inTexCoord).g;
  float sampledMetallic =
      texture(textures[material.metallicRoughnessTextureIdx], inTexCoord).b;
  vec3 sampledDiffuse =
      texture(textures[material.diffuseTextureIdx], inTexCoord).rgb;

  float roughness = material.roughness * sampledRoughness;
  float metallic = material.metallic * sampledMetallic;

  float shininess = pow(MAX_SHININESS, 1.0 - roughness);
  float normalization = ((shininess + 2.0) * (shininess + 4.0)) /
                        (8.0 * PI * (pow(2.0, -shininess * 0.5) + shininess));
  normalization = max(normalization - 0.3496155267919281, 0.0) * PI;

  float diffuseFactor = max(dot(fragToLightDir, normal), 0.0);
  float specularFactor = pow(max(dot(normal, halfDir), 0.0), shininess) *
                         diffuseFactor * normalization;

  vec3 specular = specularColor * specularFactor * SPECULAR_INTENSITY;

  vec3 diffuse =
      diffuseColor * diffuseFactor * sampledDiffuse * DIFFUSE_INTENSITY;

  vec3 ambient = ambientColor * sampledDiffuse;

  SurfaceColor color;
  color.ambient = ambient;
  color.diffuse = diffuse;
  color.specular = specular;
  color.specularFactor = specularFactor;
  color.metallicFactor = metallic;

  return color;
}

vec3 calcDirectionLight(vec3 normal, vec3 fragPos, vec3 fragToCameraDir) {
  vec3 fragToLightDir = normalize(-directionalLight.direction);
  vec3 halfDir = normalize(vec3(fragToLightDir + fragToCameraDir));

  float shadowFactor =
      calcShadow(directionalLight.lightSpaceMatrix * inPos,
                 directionalLight.shadowMapIdx) * directionalLight.shadowFactor;

  SurfaceColor surfaceColor = calcSurfaceColor(
      fragToLightDir, halfDir, normal, directionalLight.ambient,
      directionalLight.diffuse, directionalLight.specular);

  return mix(
      surfaceColor.ambient +
          ((surfaceColor.diffuse + surfaceColor.specular) * shadowFactor),
      surfaceColor.specularFactor * surfaceColor.diffuse,
      surfaceColor.metallicFactor);
}

vec3 calcPointLight(uint idx, vec3 normal, vec3 fragPos, vec3 fragToCameraDir) {
  Material material = materials.data[pushConstant.materialId];
  PointLight pointLight = pointLights.data[idx];

  vec3 lightPos = pointLight.position;
  vec3 fragToLightVec = lightPos - fragPos;
  vec3 fragToLightDir = normalize(fragToLightVec);
  vec3 halfDir = normalize(vec3(fragToLightDir + fragToCameraDir));

  SurfaceColor surfaceColor =
      calcSurfaceColor(fragToLightDir, halfDir, normal, pointLight.ambient,
                       pointLight.diffuse, pointLight.specular);

  float distance = dot(fragToLightVec, fragToLightVec);

  float attenuation =
      1.0 / (pointLight.constant + pointLight.linear * distance);

  surfaceColor.ambient *= attenuation;
  surfaceColor.diffuse *= attenuation;
  surfaceColor.specular *= attenuation;

  float shadowFactor = calcPointShadow(fragPos, idx, normal) * pointLight.shadowFactor;

  return mix(
      surfaceColor.ambient +
          ((surfaceColor.diffuse + surfaceColor.specular) * shadowFactor),
      surfaceColor.specularFactor * surfaceColor.diffuse,
      surfaceColor.metallicFactor);
}

vec3 calcSpotLight(uint idx, vec3 normal, vec3 fragPos, vec3 fragToCameraDir) {
  Material material = materials.data[pushConstant.materialId];
  SpotLight spotLight = spotLights.data[idx];

  vec3 lightPos = spotLight.position;
  vec3 fragToLightVec = lightPos - fragPos;

  vec3 lightDir = normalize(spotLight.direction);
  vec3 fragToLightDir = normalize(-fragToLightVec);

  vec3 halfDir = normalize(fragToLightDir + fragToCameraDir);
  float theta = dot(fragToLightDir, -lightDir);

  SurfaceColor surfaceColor =
      calcSurfaceColor(fragToLightDir, halfDir, normal, spotLight.ambient,
                       spotLight.diffuse, spotLight.specular);

  if (theta < spotLight.outerCutOff) {
    return surfaceColor.ambient;
  }

  float epsilon = spotLight.cutOff - spotLight.outerCutOff;
  float intensity = clamp((theta - spotLight.outerCutOff) / epsilon, 0.0, 1.0);
  float distance = dot(lightDir, lightDir);
  float attenuation = 1.0 / (spotLight.constant + spotLight.linear * distance);

  surfaceColor.diffuse *= intensity;
  surfaceColor.specular *= intensity;

  surfaceColor.ambient *= attenuation;
  surfaceColor.diffuse *= attenuation;
  surfaceColor.specular *= attenuation;

  float shadowFactor = calcShadow(spotLight.lightSpaceMatrix * inPos, spotLight.shadowMapIdx) * spotLight.shadowFactor;

  return mix(
      surfaceColor.ambient +
          ((surfaceColor.diffuse + surfaceColor.specular) * shadowFactor),
      surfaceColor.specularFactor * surfaceColor.diffuse,
      surfaceColor.metallicFactor);
}

float linearizeDepth(float depth) {
  float zNear = 0.5f;
  float zFar = 500.0f;
  return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}

vec3 skyboxReflection(vec3 diffuse, vec3 fragToCameraDir, vec3 normal) {
  Material material = materials.data[pushConstant.materialId];
  vec3 reflection = texture(cubemaps[skylight.cubemapTextureIdx],
                            reflect(-fragToCameraDir, normal))
                        .rgb;
  return mix(reflection * calcFresnelFactor(fragToCameraDir, normal) *
                 pow(1.0 - material.roughness, 2.0),
             reflection * diffuse, 1.0);
}

vec3 ACESFilm(vec3 x) {
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

vec3 Reinhard(vec3 x) {
  return x / (x + vec3(1.0));
}

void main() {
  vec3 fragPos = vec3(inPos);
  vec3 fragToCameraDir = normalize(pushConstant.camera - fragPos);
  vec3 normal = normalize(inNormal);
  Material material = materials.data[pushConstant.materialId];

  if (material.normalTextureIdx != 1) {
    vec3 tangent = normalize(inTangent.xyz);
    tangent = (tangent - dot(tangent, normal) * normal) * inTangent.w;
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    normal = texture(textures[material.normalTextureIdx], inTexCoord).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(TBN * normal);
  }

  vec3 shadow = vec3(0.0);
  shadow += calcDirectionLight(normal, fragPos, fragToCameraDir);

  for (uint i = 0; i < pushConstant.pointLights; i++) {
    shadow += calcPointLight(i, normal, fragPos, fragToCameraDir);
  }

  for (uint i = 0; i < pushConstant.spotLights; i++) {
    shadow += calcSpotLight(i, normal, fragPos, fragToCameraDir);
  }

  vec4 color = texture(textures[material.diffuseTextureIdx], inTexCoord);

  if (color.a < material.alphaCutoff) {
    discard;
  }

  color.a *= 1.0 - material.transmissionFactor;

  float c = linearizeDepth(gl_FragCoord.z);
  vec4 fog = vec4(c, c, c, 1.0);

  vec3 emissive =
      material.emissive *
      vec3(texture(textures[material.emissiveTextureIdx], inTexCoord));

  color = inColor * vec4(material.color, 1.0) * color * vec4(shadow, 1.0);

  color.xyz += emissive;
  color.xyz += skyboxReflection(color.xyz, fragToCameraDir, normal);

  color = mix(color, fog, c * 0.03);

  //outColor = vec4(ACESFilm(color.xyz * EXPOSURE), color.w);
  outColor = vec4(Reinhard(color.xyz), color.w);
}
