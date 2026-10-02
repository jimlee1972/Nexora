#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 0) out vec3 worldNormal;
layout(push_constant, row_major) uniform SceneConstants {
    mat4 mvp;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 baseColor;
} scene;
void main() {
    gl_Position = scene.mvp * vec4(position, 1.0);
    // Nexora uses column vectors and a D3D-style [0,1] depth projection.
    // Vulkan's positive-height viewport needs the clip-space Y inversion.
    gl_Position.y = -gl_Position.y;
    worldNormal = normal;
}
