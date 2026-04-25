#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D emissiveMap;   // sun surface texture
uniform float emissionStrength;  // overall brightness multiplier (e.g. 1.0)

void main()
{
    vec3 col = texture(emissiveMap, TexCoords).rgb;
    FragColor = vec4(col * emissionStrength, 1.0);
}
