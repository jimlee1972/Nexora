#version 450
layout(location=0) in vec3 illumination;
layout(location=0) out vec4 color;
void main() { color = vec4(illumination, 1.0); }
