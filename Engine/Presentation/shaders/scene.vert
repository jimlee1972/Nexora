#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 instanceTranslation;
layout(location=3) in vec3 instanceScale;
layout(location=4) in vec4 instanceColor;
layout(location=5) in vec2 uv;
layout(location=6) in vec4 instanceRotation;
layout(location=0) out vec3 illumination;
layout(location=1) out vec2 textureUv;
layout(push_constant, row_major) uniform Scene {
  mat4 mvp;
  vec4 lightDirection;
  vec4 lightColor;
  vec4 baseColor;
} scene;
void main() {
  textureUv = uv;
  vec3 scaled = position * instanceScale;
  vec3 positionCross = 2.0 * cross(instanceRotation.xyz, scaled);
  vec3 rotated = scaled + instanceRotation.w * positionCross + cross(instanceRotation.xyz, positionCross);
  gl_Position = scene.mvp * vec4(rotated + instanceTranslation, 1.0);
  gl_Position.y = -gl_Position.y;
  vec3 normalScaled = normal / instanceScale;
  vec3 normalCross = 2.0 * cross(instanceRotation.xyz, normalScaled);
  vec3 normalRotated = normalScaled + instanceRotation.w * normalCross + cross(instanceRotation.xyz, normalCross);
  float diffuse = max(dot(normalize(normalRotated), normalize(-scene.lightDirection.xyz)), 0.0);
  illumination = scene.baseColor.rgb * instanceColor.rgb * (vec3(0.18) + scene.lightColor.rgb * diffuse * 0.82);
}
