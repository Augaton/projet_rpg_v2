#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 renderSize;

out vec4 finalColor;

void main()
{
    // Sécurité : si renderSize est 0, on prend une valeur par défaut pour éviter le crash
    vec2 res = (renderSize.x <= 0.0) ? vec2(800.0, 600.0) : renderSize;
    vec2 texelSize = 1.0 / res;
    
    // On utilise un décalage de 0.5 texel pour être SUR de lire au centre des pixels
    // C'est ce qui supprime les lignes verticales/horizontales parasites
    vec2 uv = fragTexCoord;

    vec4 sum = vec4(0.0);
    
    // Échantillonnage en "X" (plus stable sur Intel que le carré 3x3)
    sum += texture(texture0, uv) * 0.4;
    sum += texture(texture0, uv + vec2(texelSize.x, texelSize.y)) * 0.15;
    sum += texture(texture0, uv - vec2(texelSize.x, texelSize.y)) * 0.15;
    sum += texture(texture0, uv + vec2(texelSize.x, -texelSize.y)) * 0.15;
    sum += texture(texture0, uv - vec2(texelSize.x, -texelSize.y)) * 0.15;
    
    finalColor = sum * fragColor * colDiffuse;
}