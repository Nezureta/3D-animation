#version 330 core
out vec4 FragColor;

in  vec2 TexCoords;
in  vec3 FragPos;
in  vec3 Normal;

uniform sampler2D ringTexture;
uniform vec3      lightPos;     // sun position (world space)
uniform vec3      lightColor;   // sun's diffuse colour
uniform vec3      ambientColor; // small ambient term so dark side isn't pure black

void main()
{
    vec3 sample = texture(ringTexture, TexCoords).rgb;

    // The Solar System Scope "ring_alpha" texture is grayscale RGB; luminance
    // doubles as the opacity (bright bands of ring particles = opaque, gaps = thin).
    float lum = dot(sample, vec3(0.299, 0.587, 0.114));

    // Simple double-sided Lambert from the sun. The ring is a flat double-sided
    // disc, so we take |N . L| rather than max(0, N . L).
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);
    float diff = abs(dot(N, L));

    vec3 color = sample * (ambientColor + diff * lightColor);
    FragColor  = vec4(color, lum);
}
