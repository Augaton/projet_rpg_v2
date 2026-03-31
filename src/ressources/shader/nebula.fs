#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform float iTime;
uniform vec2  iResolution;
uniform float iFTL;       // 0.0 à 1.0 (ton ftlLerp)
uniform float iImpact;    // Flash blanc lors des dégâts
uniform float iSeed;      // Graine du secteur
uniform vec3  iColorPrimary;
uniform vec3  iColorSecondary;

// --- UTILS PROCÉDURAUX ---
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    float a = hash(i);
    float b = hash(i+vec2(1,0));
    float c = hash(i+vec2(0,1));
    float d = hash(i+vec2(1,1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0; float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.0; a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 fragPos = uv * iResolution;

    // 1. FOND : NÉBULEUSE PROCÉDURALE
    // On déplace le fond légèrement avec le temps pour donner de la vie
    vec2 p = uv * vec2(iResolution.x/iResolution.y, 1.0);
    p += iSeed; // Décale les formes selon le secteur
    p.x -= iTime * 0.05; // Dérive lente
    
    float n = fbm(p * 2.0);
    float cloud = pow(n, 3.0) * 1.5; // Contraste des gaz
    vec3 nebula = mix(iColorPrimary, iColorSecondary, n) * cloud;

    // 2. LOGIQUE ÉTOILES FTL (TON ANCIEN CODE PRÉSERVÉ)
    float nx = fragPos.x + iTime * 40.0;
    float ny = fragPos.y + sin(iTime * 0.3) * 30.0;
    
    // Adaptation de ton bruit original
    float n_noise = (sin(nx * 0.02 + iTime * 0.2) + 
                     cos(ny * 0.02 - iTime * 0.15) + 
                     sin((fragPos.x + fragPos.y) * 0.01 + iTime * 0.1)) * 0.5;
                     
    float starIntensity = pow((n_noise + 1.0) * 0.5, 2.0);
    float gradX = uv.x;

    float r = (0.6 + 0.4 * sin(starIntensity * 3.0 + iTime)) * gradX;
    float g = (0.5 + 0.5 * sin(starIntensity * 2.0 + iTime * 0.7)) * (1.0 - gradX);
    float b = (0.7 + 0.3 * sin(starIntensity * 4.0 - iTime * 0.5));
    
    vec4 starCol = vec4(r, g, b, 0.137);

    // 3. ASSEMBLAGE
    // On ajoute un fond spatial très sombre derrière la nébuleuse
    vec3 finalRGB = mix(vec3(0.005, 0.005, 0.01), nebula, 0.8);
    
    // On mélange avec tes étoiles FTL
    finalRGB = finalRGB * (1.0 - starCol.a) + starCol.rgb * starCol.a;

    // Effet d'impact (flash blanc)
    finalRGB += vec3(iImpact * 0.3);

    finalColor = vec4(finalRGB, 1.0);
}