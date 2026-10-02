#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 instanceTranslation;
layout(location=3) in vec3 instanceScale;
layout(location=4) in vec4 instanceColor;
layout(location=0) out vec3 illumination;
layout(push_constant, row_major) uniform Scene {
  mat4 mvp;
  vec4 lightDirection;
  vec4 lightColor;
  vec4 baseColor;
} scene;
void main() {
  gl_Position = scene.mvp * vec4(position * instanceScale + instanceTranslation, 1.0);
  gl_Position.y = -gl_Position.y;
  float diffuse = max(dot(normalize(normal / instanceScale), normalize(-scene.lightDirection.xyz)), 0.0);
  illumination = scene.baseColor.rgb * instanceColor.rgb * (vec3(0.18) + scene.lightColor.rgb * diffuse * 0.82);
}
