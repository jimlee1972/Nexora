#version 450
layout(location=0) in vec2 origin;
layout(location=1) in vec2 axisX;
layout(location=2) in vec2 axisY;
layout(location=3) in vec4 uv;
layout(location=4) in vec4 color;
layout(location=0) out vec2 textureUV;
layout(location=1) out vec4 tint;
layout(push_constant) uniform Transform { vec2 scale; vec2 translate; } transform;
void main() {
  const vec2 corners[6] = vec2[](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
  vec2 corner = corners[gl_VertexIndex];
  vec2 position = origin + axisX * corner.x + axisY * corner.y;
  gl_Position = vec4(position * transform.scale + transform.translate, 0.0, 1.0);
  textureUV = mix(uv.xy,uv.zw,corner);
  tint = color;
}
