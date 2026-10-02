#version 450
layout(location=0) in vec3 illumination;
layout(location=1) in vec2 textureUv;
layout(set=0,binding=0) uniform sampler2D materialTexture;
layout(location=0) out vec4 color;
void main() { color = vec4(illumination, 1.0) * texture(materialTexture, textureUv); }
