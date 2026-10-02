#version 450
layout(location=0) in vec2 textureUV;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 color;
layout(set=0, binding=0) uniform sampler2D atlas;
void main() { color = tint * texture(atlas, textureUV); }
