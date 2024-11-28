#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);

    vec3 toLight = normalize(vec3(2.0, 3.0, 1.0));

    float min_lightness = 0.5;
    float lightness = min_lightness + ((dot(normalize(fragNormal), toLight) + 1.0) * 0.5) * (1.0 - min_lightness);

    finalColor = texelColor * (colDiffuse * lightness);
    finalColor.a = 1.0;
}
