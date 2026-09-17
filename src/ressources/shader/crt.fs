#version 330

// Composition finale « écran cathodique » : courbure, aberration chromatique,
// lignes de balayage, masque phosphore, vignette et grain.
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2  sourceSize;   // canevas virtuel (1280x720)
uniform float time;
uniform float intensity;    // 0 = désactivé, 1 = fort
uniform float aberration;   // en pixels virtuels

out vec4 finalColor;

vec2 Curve(vec2 uv, float k)
{
    uv = uv * 2.0 - 1.0;
    vec2 offset = abs(uv.yx) / vec2(5.5, 4.5);
    uv = uv + uv * offset * offset * k;
    return uv * 0.5 + 0.5;
}

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = Curve(fragTexCoord, intensity * 0.7);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float ab = aberration / sourceSize.x;
    vec3 col = vec3(
        texture(texture0, uv + vec2(ab, 0.0)).r,
        texture(texture0, uv).g,
        texture(texture0, uv - vec2(ab, 0.0)).b
    );

    // Une ligne de balayage par ligne du canevas virtuel
    float scan = 0.5 + 0.5 * cos(uv.y * sourceSize.y * 6.2831853);
    col *= mix(1.0, 0.70 + 0.30 * scan, intensity);

    // Masque de phosphore RVB (à la résolution de sortie)
    int triad = int(mod(gl_FragCoord.x, 3.0));
    vec3 mask = vec3(triad == 0 ? 1.0 : 0.82, triad == 1 ? 1.0 : 0.82, triad == 2 ? 1.0 : 0.82);
    col *= mix(vec3(1.0), mask, intensity * 0.6);

    // Vignette
    vec2 v = uv * (1.0 - uv.yx);
    float vig = clamp(pow(v.x * v.y * 18.0, 0.22), 0.0, 1.0);
    col *= mix(1.0, vig, intensity);

    // Grain + léger scintillement
    col += (Hash(gl_FragCoord.xy + fract(time) * 100.0) - 0.5) * 0.05 * intensity;
    col *= 1.0 + intensity * (0.22 + 0.01 * sin(time * 60.0));

    finalColor = vec4(col, 1.0);
}
