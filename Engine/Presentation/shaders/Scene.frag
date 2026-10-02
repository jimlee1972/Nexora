#version 450
layout(location = 0) in vec3 worldNormal;
layout(location = 0) out vec4 color;
layout(push_constant, row_major) uniform SceneConstants {
    mat4 mvp;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 baseColor;
} scene;
vec3 safeNormalize(vec3 value) {
    return value * inversesqrt(max(dot(value, value), 1e-12));
}
void main() {
    float diffuse = max(dot(safeNormalize(worldNormal), safeNormalize(-scene.lightDirection.xyz)), 0.0);
    color = vec4(scene.baseColor.rgb * (vec3(0.15) + scene.lightColor.rgb * diffuse), scene.baseColor.a);
}
