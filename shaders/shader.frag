#version 450 

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D tex0;

void main() {
  outColor = texture(tex0, inTexCoord);
  //outColor = vec4(inColor, 1.0f);
}
