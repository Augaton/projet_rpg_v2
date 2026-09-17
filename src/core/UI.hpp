#pragma once
#include "raylib.h"

// Boîte à outils d'interface — style rétro « brut » :
// blocs à angles vifs, bordures épaisses, ombres portées dures,
// police pixel pour les titres, police terminal pour le texte courant.
namespace ui {

// ─── Palette (phosphore ambre / vert / cyan sur noir) ────────────────────────

inline constexpr Color C_HULL_FULL   = {  90, 255, 130, 255 };
inline constexpr Color C_HULL_MID    = { 255, 190,   0, 255 };
inline constexpr Color C_HULL_LOW    = { 255,  60,  50, 255 };
inline constexpr Color C_HULL_TRAIL  = { 255, 140, 110, 210 };
inline constexpr Color C_SHIELD      = {  60, 220, 255, 255 };
inline constexpr Color C_ION         = { 190, 120, 255, 255 };
inline constexpr Color C_FUEL        = { 255, 170,   0, 255 };
inline constexpr Color C_SCRAP       = { 255, 220,  90, 255 };
inline constexpr Color C_MISSILE     = { 255, 110,  50, 255 };
inline constexpr Color C_POWER       = {  90, 255, 130, 255 };
inline constexpr Color C_BAR_BG      = {  22,  22,  24, 255 };
inline constexpr Color C_PANEL_BG    = {   8,   8,  10, 238 };
inline constexpr Color C_PANEL_BG_EN = {  22,   6,   6, 238 };
inline constexpr Color C_BORDER      = {  78,  76,  70, 255 };
inline constexpr Color C_LABEL       = { 172, 168, 156, 255 };
inline constexpr Color C_TEXT        = { 238, 234, 222, 255 };
inline constexpr Color C_TEXT_DIM    = { 118, 115, 106, 255 };
inline constexpr Color C_ACCENT      = { 255, 176,   0, 255 };
inline constexpr Color C_ENEMY       = { 255,  60,  50, 255 };
inline constexpr Color C_WARNING     = { 255, 176,   0, 255 };
inline constexpr Color C_GOOD        = {  90, 255, 130, 255 };
inline constexpr Color C_INK         = {   0,   0,   0, 255 };

// ─── Polices ─────────────────────────────────────────────────────────────────

void  LoadFont();
void  UnloadFont();

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

// Titres et étiquettes (police pixel, taille arrondie au multiple de 10)
float MeasureLabel(const char* text, float size);
void  Label(const char* text, float x, float y, float size, Color color);
void  LabelCentered(const char* text, float cx, float y, float size, Color color);
void  LabelRight(const char* text, float rightX, float y, float size, Color color);

// ─── Formes ──────────────────────────────────────────────────────────────────

Color LerpColor(Color a, Color b, float t);
void  CornerBrackets(Rectangle r, float size, Color color);
// Bloc avec ombre dure, bordure épaisse et onglet de titre optionnel
void  Panel(Rectangle r, Color accent, float accentAlpha = 0.0f, Color bg = C_PANEL_BG,
            const char* title = nullptr);
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
