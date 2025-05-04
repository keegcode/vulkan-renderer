#version 450

layout(location = 0) in vec3 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube skybox;

float linearizeDepth(float depth) {
	float zNear = 0.5f; 
	float zFar  = 500.0f;
	return (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
}

void main() {
	float c = linearizeDepth(gl_FragCoord.z);
	vec4 fog = vec4(c, c, c, 1.0);

	outColor = mix(texture(skybox, inTexCoord), fog, c * 0.05);
}
