// resources/shaders/nebula.fs
#version 330

// Entrées standard de Raylib
in vec2 fragTexCoord;
in vec4 fragColor;

// Nos "Uniforms" custom envoyés depuis le C++
uniform float time;
uniform vec2 renderSize;

// Sortie : la couleur finale du pixel
out vec4 finalColor;

// Fonction de bruit pseudo-aléatoire rapide sur GPU
float rand(vec2 n) { 
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

// Fonction de bruit (similaire à la tienne, adaptée GPU)
float noise(vec2 p, float t) {
    vec2 nx = vec2(p.x * 0.02 + t * 0.2, p.y * 0.02 - t * 0.15);
    float n = sin(nx.x) + cos(nx.y) + sin((p.x + p.y) * 0.01 + t * 0.1);
    return n * 0.5;
}

void main() {
    // Récupérer les coordonnées de pixel réelles (0 à Width, 0 à Height)
    vec2 fragPos = fragTexCoord * renderSize;
    
    // Ton ancienne logique de bruit transposée ici
    float nx = fragPos.x + time * 40.0;
    float ny = fragPos.y + sin(time * 0.3) * 30.0;

    float n = noise(vec2(nx, ny), time);
    n = pow((n + 1.0) * 0.5, 2.0); // Contraste

    float gradient = fragTexCoord.x; // Équivalent à x / screenW

    // Calcul des couleurs R, G, B
    float r = (0.6 + 0.4 * sin(n * 3.0 + time)) * gradient;
    float g = (0.5 + 0.5 * sin(n * 2.0 + time * 0.7)) * (1.0 - gradient);
    float b = (0.7 + 0.3 * sin(n * 4.0 - time * 0.5));

    // Couleur finale avec l'alpha d'origine (35/255 ≈ 0.137)
    finalColor = vec4(r, g, b, 0.137);
}