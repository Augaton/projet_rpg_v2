#version 330

// Flou gaussien séparable (9 taps linéaires) — une passe horizontale, une verticale
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 direction;   // (1,0) ou (0,1)
uniform vec2 texelSize;   // 1 / taille de la cible

out vec4 finalColor;

void main()
{
    vec2 o1 = direction * texelSize * 1.3846153846;
    vec2 o2 = direction * texelSize * 3.2307692308;

    vec4 sum = texture(texture0, fragTexCoord) * 0.2270270270;
    sum += texture(texture0, fragTexCoord + o1) * 0.3162162162;
    sum += texture(texture0, fragTexCoord - o1) * 0.3162162162;
    sum += texture(texture0, fragTexCoord + o2) * 0.0702702703;
    sum += texture(texture0, fragTexCoord - o2) * 0.0702702703;

    finalColor = sum;
}
