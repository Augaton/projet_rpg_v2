#pragma once
#include "raylib.h"

// Boîte à outils d'interface — style « rétro net » :
// consoles à coins coupés sur noir chaud, filet d'accent en haut, pas de contours lourds,
// Chakra Petch pour les titres, police terminal pour le texte courant.
namespace ui {

// ─── Palette (ambre / vert / cyan sur noir chaud) ────────────────────────────

inline constexpr Color C_HULL_FULL   = { 108, 227, 140, 255 };
inline constexpr Color C_HULL_MID    = { 255, 176,   0, 255 };
inline constexpr Color C_HULL_LOW    = { 255,  90,  76, 255 };
inline constexpr Color C_HULL_TRAIL  = { 255, 150, 120, 210 };
inline constexpr Color C_SHIELD      = {  63, 208, 232, 255 };
inline constexpr Color C_ION         = { 179, 156, 255, 255 };
inline constexpr Color C_FUEL        = { 255, 170,   0, 255 };
inline constexpr Color C_SCRAP       = { 255, 210,  90, 255 };
inline constexpr Color C_MISSILE     = { 255, 120,  60, 255 };
inline constexpr Color C_POWER       = { 108, 227, 140, 255 };
inline constexpr Color C_BAR_BG      = {  44,  40,  34, 255 };
inline constexpr Color C_PANEL_BG    = {  21,  19,  15, 240 };
inline constexpr Color C_PANEL_BG_EN = {  30,  14,  12, 240 };
inline constexpr Color C_PANEL_2     = {  28,  25,  20, 255 };   // boutons, lignes
inline constexpr Color C_BORDER      = {  74,  68,  58, 255 };
inline constexpr Color C_LABEL       = { 200, 192, 174, 255 };
inline constexpr Color C_TEXT        = { 242, 235, 221, 255 };
inline constexpr Color C_TEXT_DIM    = { 171, 163, 146, 255 };
inline constexpr Color C_ACCENT      = { 255, 176,   0, 255 };
inline constexpr Color C_ENEMY       = { 255,  90,  76, 255 };
inline constexpr Color C_WARNING     = { 255, 176,   0, 255 };
inline constexpr Color C_GOOD        = { 108, 227, 140, 255 };
inline constexpr Color C_INK         = {  21,  17,  10, 255 };

// ─── Ressources ──────────────────────────────────────────────────────────────

// Polices, icônes et logo (asset/fonts, asset/ui)
void  Load();
void  Unload();

// Texte courant (police terminal)
float Measure(const char* text, float size);
void  Text(const char* text, float x, float y, float size, Color color);
void  TextCentered(const char* text, float cx, float y, float size, Color color);
void  TextRight(const char* text, float rightX, float y, float size, Color color);
// Retour à la ligne automatique ; maxChars >= 0 limite le nombre de caractères
// affichés (effet machine à écrire). Renvoie la hauteur totale du texte.
float TextWrapped(const char* text, Rectangle box, float size, Color color,
                  bool centered = false, bool draw = true, int maxChars = -1);
int   CharCount(const char* text);

// Titres et étiquettes (Chakra Petch ; taille arrondie au multiple de 10,
// mêmes encombrements que l'ancienne police pixel)
float MeasureLabel(const char* text, float size);
void  Label(const char* text, float x, float y, float size, Color color);
void  LabelCentered(const char* text, float cx, float y, float size, Color color);
void  LabelRight(const char* text, float rightX, float y, float size, Color color);

// ─── Icônes (asset/ui/icons.png, généré par tools/gen_ui_assets.py) ─────────

// Les huit premières suivent l'ordre des systèmes : l'icône d'un système est son indice.
enum Icon {
    ICON_SHIELD, ICON_ENGINES, ICON_WEAPONS, ICON_REPAIR,
    ICON_CLOAK, ICON_HACKING, ICON_DEFENSE, ICON_SENSORS,
    ICON_HULL, ICON_FUEL, ICON_MISSILE, ICON_SCRAP, ICON_POWER,
    ICON_LASER, ICON_CANNON, ICON_ION, ICON_WARNING,
    ICON_COUNT
};
inline constexpr float ICON_SIZE = 16.0f;

// Icône centrée en c, agrandie d'un facteur entier, teintée
void  DrawIcon(int icon, Vector2 c, float scale, Color tint);
// Logo du titre, coin haut-gauche en (x, y) ; renvoie sa taille à l'écran
Vector2 DrawLogo(float x, float y, float scale, Color tint = WHITE);

// ─── Formes ──────────────────────────────────────────────────────────────────

Color LerpColor(Color a, Color b, float t);
void  CornerBrackets(Rectangle r, float size, Color color);
// Bloc à coins coupés (haut-gauche et bas-droit) : la signature des panneaux
void  Bevel(Rectangle r, float cut, Color fill);
void  BevelLines(Rectangle r, float cut, Color color, float thick = 1.0f);
// Jauge segmentée (fill 0..1)
void  SegBar(Rectangle r, float fill, int segments, Color on, Color off);
// Bloc avec ombre dure, bordure épaisse et onglet de titre optionnel
void  Panel(Rectangle r, Color accent, float accentAlpha = 0.0f, Color bg = C_PANEL_BG,
            const char* title = nullptr);
// Console de HUD : variante légère et translucide du panneau
void  Console(Rectangle r, Color accent, const char* tab = nullptr, Color tabColor = C_LABEL);
void  Hazard(Rectangle r, Color color, float offset = 0.0f);  // bandes diagonales
void  Dim(float alpha);

// ─── Interaction ─────────────────────────────────────────────────────────────

// À appeler une fois par frame, avant tout bouton (navigation clavier)
void  BeginFrame();
void  ResetFocus();
// Son joué à chaque activation d'un bouton ou réglage
void  SetClickSound(void (*callback)(void* user), void* user);
// Désactive les interactions (ex. : menu pause par-dessus un écran)
void  SetInputEnabled(bool enabled);
bool  Hover(Rectangle r);
bool  Clicked(Rectangle r, int button = MOUSE_BUTTON_LEFT);

// Bouton : clic souris ou flèches + Entrée
bool  Button(Rectangle r, const char* label, bool enabled = true,
             Color accent = C_ACCENT, float size = 20.0f);
// Variante texte courant, alignée à gauche (choix d'événements)
bool  ButtonText(Rectangle r, const char* label, bool enabled = true,
                 Color accent = C_ACCENT, float size = 18.0f);
// Ligne de réglage « < valeur > » : renvoie -1, 0 ou +1
int   Stepper(Rectangle r, const char* label, const char* value, Color accent = C_ACCENT);

} // namespace ui
