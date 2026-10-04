#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 modelRow0;
layout(location=4) in vec4 modelRow1;
layout(location=5) in vec4 modelRow2;
layout(location=6) in vec4 normalRow0;
layout(location=7) in vec4 normalRow1;
layout(location=8) in vec4 normalRow2;
layout(location=9) in vec4 instanceColor;
layout(location=0) out vec3 illumination;
layout(location=1) out vec2 textureUv;
layout(push_constant, row_major) uniform Scene {
  mat4 mvp;
  vec4 lightDirection;
  vec4 lightColor;
  vec4 baseColor;
} scene;
vec3 safeNormal(vec3 value) {
  float magnitude = max(max(abs(value.x), abs(value.y)), abs(value.z));
  return magnitude > 0.0 ? normalize(value / magnitude) : vec3(0.0);
}
void main() {
  textureUv = uv;
  vec4 localPosition = vec4(position, 1.0);
  vec3 worldPosition = vec3(dot(modelRow0, localPosition), dot(modelRow1, localPosition), dot(modelRow2, localPosition));
  gl_Position = scene.mvp * vec4(worldPosition, 1.0);
  gl_Position.y = -gl_Position.y;
  vec3 localNormal = safeNormal(normal);
  vec3 normalRotated = vec3(dot(normalRow0.xyz, localNormal), dot(normalRow1.xyz, localNormal), dot(normalRow2.xyz, localNormal));
  float diffuse = max(dot(safeNormal(normalRotated), safeNormal(-scene.lightDirection.xyz)), 0.0);
  illumination = scene.baseColor.rgb * instanceColor.rgb * (vec3(0.18) + scene.lightColor.rgb * diffuse * 0.82);
}
