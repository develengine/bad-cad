#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 normalMatrix;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = vec3(normalMatrix * vec4(vertexNormal, 0.0));

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
