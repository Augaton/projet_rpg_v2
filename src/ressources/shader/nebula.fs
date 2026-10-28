#version 330

// Fond spatial procédural : nébuleuse, soleil hors champ et planète propres à chaque balise.
// Rendu en basse résolution puis agrandi : le tramage ordonné donne le grain rétro.
// Statique : calculé une seule fois par balise (le saut FTL et les impacts sont superposés à part).
in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec2  iResolution;
uniform float iSeed;      // graine de la balise
uniform vec3  iColorPrimary;
uniform vec3  iColorSecondary;
uniform float iLevels;    // > 0 : quantification + tramage ordonné

float bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    const float m[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                  3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    return m[x + y * 4] / 16.0;
}

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float hash1(float n) { return fract(sin(n * 127.1) * 43758.5453); }

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    const mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p = rot * p * 2.03;
        a *= 0.5;
    }
    return v;
}

void main() {
    float aspect = iResolution.x / iResolution.y;
    vec2 p = vec2(fragTexCoord.x * aspect, fragTexCoord.y);

    // Soleil hors champ : sa direction éclaire la planète
    float sa = hash1(iSeed) * 6.2831853;
    vec2 sunPos = vec2(aspect * 0.5, 0.5) + vec2(cos(sa) * aspect * 0.62, sin(sa) * 0.62);
    vec3 sunCol = mix(vec3(1.0, 0.86, 0.62), iColorPrimary, 0.3);

    // ── Nébuleuse : bruit fractal à domaine déformé, cœurs lumineux, poussière sombre
    vec2 q = p * 1.5 + iSeed;
    vec2 w = vec2(fbm(q + vec2(0.0, 1.7)), fbm(q + vec2(5.2, 1.3)));
    float n = fbm(q + 1.7 * w);
    float dust = fbm(q * 2.4 + w + 9.1);

    float cloud = smoothstep(0.32, 0.92, n);
    vec3 neb = mix(iColorSecondary, iColorPrimary, smoothstep(0.35, 0.85, w.x)) * cloud;
    neb += mix(iColorPrimary, vec3(1.0), 0.25) * pow(cloud, 4.0) * 0.7;
    neb *= 1.0 - smoothstep(0.45, 0.78, dust) * 0.8;

    vec3 col = vec3(0.006, 0.006, 0.016) + neb * 0.62;

    // Halo du soleil
    float sd = length(p - sunPos);
    col += sunCol * (0.30 * exp(-sd * 2.6) + 0.06 * exp(-sd * 0.8));

    // ── Planète (absente d'une balise sur trois)
    if (hash1(iSeed + 3.1) > 0.33) {
        const vec2 anchors[4] = vec2[4](vec2(0.86, 0.86), vec2(0.10, 0.20), vec2(0.78, 0.08), vec2(0.20, 0.94));
        vec2 pc = anchors[int(hash1(iSeed + 7.7) * 3.999)];
        pc.x *= aspect;
        float pr = mix(0.17, 0.33, hash1(iSeed + 2.3));
        vec2 d = (p - pc) / pr;
        float rr = length(d);
        vec2 l2 = normalize(sunPos - pc);
        vec3 atmCol = mix(vec3(0.5, 0.75, 1.0), iColorPrimary, 0.45);

        if (rr < 1.0) {
            vec3 N = vec3(d, sqrt(1.0 - rr * rr));
            vec3 L = normalize(vec3(l2, 0.35));
            float ndl = dot(N, L);
            float light = smoothstep(-0.08, 0.35, ndl);

            // Bandes de géante gazeuse ou relief rocheux selon la graine
            float gas = step(0.5, hash1(iSeed + 4.4));
            vec2 sp = N.xy / (N.z + 0.6) * 1.6 + iSeed * 3.0;
            float surf = mix(fbm(sp * 2.2), fbm(vec2(sp.x * 0.6, sp.y * 7.0)), gas);
            vec3 base = mix(iColorSecondary * 1.4 + 0.04, iColorPrimary * 0.9 + 0.06, hash1(iSeed + 5.0));
            vec3 surfCol = base * (0.45 + 0.9 * surf);

            float rim = pow(1.0 - N.z, 3.0);
            vec3 planet = surfCol * light * 0.85 + atmCol * rim * (0.15 + 0.85 * light) * 0.9;
            planet += surfCol * 0.02;   // face nocturne à peine visible
            float edge = smoothstep(1.0, 0.97, rr);
            col = mix(col, planet, edge);
        } else {
            // Atmosphère diffuse côté soleil
            float side = 0.35 + 0.65 * max(0.0, dot(normalize(d), l2));
            col += atmCol * exp(-(rr - 1.0) * 14.0) * side * 0.45;
        }
    }

    // Quantification tramée en espace gamma : pas fins dans les ombres, pas de damier
    if (iLevels > 0.0) {
        vec3 g = sqrt(max(col, 0.0));
        g = floor(g * iLevels + bayer4(gl_FragCoord.xy)) / iLevels;
        col = g * g;
    }

    finalColor = vec4(col, 1.0);
}
