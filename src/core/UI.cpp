#include "UI.hpp"
#include "Config.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui {

static Font s_mono;
static bool s_monoLoaded = false;

// ═════════════════════════════════════════════════════════════════════════════
// Polices
// ═════════════════════════════════════════════════════════════════════════════

void LoadFont() {
    // ASCII + Latin-1 pour les accents français, plus œ Œ ’ — …
    int codepoints[229];
    for (int i = 0; i < 224; i++) codepoints[i] = 32 + i;
    codepoints[224] = 0x153;
    codepoints[225] = 0x152;
    codepoints[226] = 0x2019;
    codepoints[227] = 0x2014;
    codepoints[228] = 0x2026;

    const char* path = "asset/fonts/SourceCodePro-Semibold.otf";
    if (FileExists(path)) {
        Font f = LoadFontEx(path, 40, codepoints, 229);
        if (IsFontValid(f) && f.texture.id > 0) {
            s_mono = f;
            s_monoLoaded = true;
            GenTextureMipmaps(&s_mono.texture);
            SetTextureFilter(s_mono.texture, TEXTURE_FILTER_TRILINEAR);
            return;
        }
    }
    TraceLog(LOG_WARNING, "UI: police terminal introuvable, repli sur la police pixel");
    s_mono = GetFontDefault();
}

void UnloadFont() {
    if (s_monoLoaded) ::UnloadFont(s_mono);
    s_monoLoaded = false;
}

// ─── Texte courant ───────────────────────────────────────────────────────────

float Measure(const char* text, float size) {
    return MeasureTextEx(s_mono, text, size, 0.0f).x;
}

void Text(const char* text, float x, float y, float size, Color color) {
    DrawTextEx(s_mono, text, { std::round(x), std::round(y) }, size, 0.0f, color);
}

void TextCentered(const char* text, float cx, float y, float size, Color color) {
    Text(text, cx - Measure(text, size) * 0.5f, y, size, color);
}

void TextRight(const char* text, float rightX, float y, float size, Color color) {
    Text(text, rightX - Measure(text, size), y, size, color);
}

int CharCount(const char* text) { return GetCodepointCount(text); }

// Découpage en lignes mis en cache (évite de re-mesurer chaque frame)
static const std::vector<std::string>& WrapLines(const char* text, float width, float size) {
    static std::unordered_map<std::string, std::vector<std::string>> cache;

    // Clé construite dans un tampon réutilisé : pas d'allocation quand le texte est en cache
    static std::string key;
    key.assign(text);
    char suffix[32];
    int n = std::snprintf(suffix, sizeof suffix, "\x1f%d\x1f%d", (int)size, (int)width);
    key.append(suffix, (size_t)std::clamp(n, 0, (int)sizeof suffix - 1));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 512) cache.clear();

    std::vector<std::string> lines;
    std::string src(text);
    size_t paraStart = 0;

    while (paraStart <= src.size()) {
        size_t paraEnd = src.find('\n', paraStart);
        if (paraEnd == std::string::npos) paraEnd = src.size();
        std::string para = src.substr(paraStart, paraEnd - paraStart);

        std::string line;
        size_t pos = 0;
        while (pos <= para.size()) {
            size_t sp = para.find(' ', pos);
            if (sp == std::string::npos) sp = para.size();
            std::string word = para.substr(pos, sp - pos);
            std::string candidate = line.empty() ? word : line + " " + word;

            if (!line.empty() && Measure(candidate.c_str(), size) > width) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
            pos = sp + 1;
        }
        lines.push_back(line);
        paraStart = paraEnd + 1;
    }

    return cache.emplace(key, std::move(lines)).first->second;
}

float TextWrapped(const char* text, Rectangle box, float size, Color color,
                  bool centered, bool draw, int maxChars) {
    const float lineH = size * 1.35f;
    const auto& lines = WrapLines(text, box.width, size);

    if (draw) {
        int budget = maxChars;
        float y = box.y;
        for (const auto& line : lines) {
            if (budget == 0) break;
            const char* shown = line.c_str();
            if (budget > 0) {
                int count = GetCodepointCount(line.c_str());
                if (count > budget) {
                    // Coupe sans casser une séquence UTF-8
                    const char* p = line.c_str();
                    size_t bytes = 0;
                    for (int i = 0; i < budget && bytes < line.size(); i++) {
                        int sz = 0;
                        GetCodepointNext(p + bytes, &sz);
                        bytes += (size_t)std::max(sz, 1);
                    }
                    static std::string cut;
                    cut.assign(line, 0, std::min(bytes, line.size()));
                    shown = cut.c_str();
                    count = budget;
                }
                budget -= std::min(budget, count + 1);   // +1 pour le saut de ligne
            }
            if (centered) TextCentered(shown, box.x + box.width * 0.5f, y, size, color);
            else          Text(shown, box.x, y, size, color);
            y += lineH;
        }
    }
    return lineH * (float)lines.size();
}

// ─── Étiquettes pixel ────────────────────────────────────────────────────────

static float SnapPixel(float size) { return std::max(10.0f, std::round(size / 10.0f) * 10.0f); }

// La police pixel ne couvre que Latin-1 : on remplace les rares caractères hors plage
static const char* PixelSafe(const char* text) {
    static std::string buf;
    buf.clear();
    for (const char* p = text; *p; ) {
        unsigned char c = (unsigned char)*p;
        if (c == 0xC5 && (unsigned char)p[1] == 0x93) { buf += "oe"; p += 2; continue; }
        if (c == 0xC5 && (unsigned char)p[1] == 0x92) { buf += "OE"; p += 2; continue; }
        if (c == 0xE2 && (unsigned char)p[1] == 0x80 && p[2]) {
            unsigned char d = (unsigned char)p[2];
            if (d == 0x94 || d == 0x93) { buf += "-";   p += 3; continue; }
            if (d == 0x99)              { buf += "'";   p += 3; continue; }
            if (d == 0xA6)              { buf += "..."; p += 3; continue; }
        }
        buf += *p++;
    }
    return buf.c_str();
}

float MeasureLabel(const char* text, float size) {
    float s = SnapPixel(size);
    return MeasureTextEx(GetFontDefault(), PixelSafe(text), s, s / 10.0f).x;
}

void Label(const char* text, float x, float y, float size, Color color) {
    float s = SnapPixel(size);
    DrawTextEx(GetFontDefault(), PixelSafe(text), { std::round(x), std::round(y) }, s, s / 10.0f, color);
}

void LabelCentered(const char* text, float cx, float y, float size, Color color) {
    Label(text, cx - MeasureLabel(text, size) * 0.5f, y, size, color);
}

void LabelRight(const char* text, float rightX, float y, float size, Color color) {
    Label(text, rightX - MeasureLabel(text, size), y, size, color);
}

// ═════════════════════════════════════════════════════════════════════════════
// Formes
// ═════════════════════════════════════════════════════════════════════════════

Color LerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) {
        return (unsigned char)((float)x + ((float)y - (float)x) * t);
    };
    return { l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a) };
}

void CornerBrackets(Rectangle r, float s, Color c) {
    const float th = 2.0f;
    float x0 = r.x, y0 = r.y, x1 = r.x + r.width - th, y1 = r.y + r.height - th;
    DrawRectangleRec({ x0, y0, s, th }, c);          DrawRectangleRec({ x0, y0, th, s }, c);
    DrawRectangleRec({ x1 - s + th, y0, s, th }, c); DrawRectangleRec({ x1, y0, th, s }, c);
    DrawRectangleRec({ x0, y1, s, th }, c);          DrawRectangleRec({ x0, y1 - s + th, th, s }, c);
    DrawRectangleRec({ x1 - s + th, y1, s, th }, c); DrawRectangleRec({ x1, y1 - s + th, th, s }, c);
}

void Panel(Rectangle r, Color accent, float accentAlpha, Color bg, const char* title) {
    DrawRectangleRec({ r.x + 4, r.y + 4, r.width, r.height }, ColorAlpha(BLACK, 0.3f));
    DrawRectangleRec(r, ColorAlpha(bg, std::min(bg.a / 255.0f, 0.88f)));
    DrawRectangleLinesEx(r, 1.0f, LerpColor(ColorAlpha(C_BORDER, 0.8f), accent, 0.35f + accentAlpha));

    // Filet d'accent en haut
    DrawRectangleRec({ r.x, r.y, r.width, 2 }, ColorAlpha(accent, 0.8f));

    if (title) {
        float w = MeasureLabel(title, 10) + 14;
        DrawRectangleRec({ r.x + 14, r.y - 6, w, 13 }, accent);
        Label(title, r.x + 21, r.y - 4, 10, C_INK);
    }
}

void Hazard(Rectangle r, Color color, float offset) {
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    const float step = 14.0f;
    float start = r.x - r.height - step * 2.0f + std::fmod(offset, step * 2.0f);
    for (float x = start; x < r.x + r.width + r.height; x += step * 2.0f) {
        Vector2 a = { x, r.y + r.height },            b = { x + step, r.y + r.height };
        Vector2 c = { x + step + r.height, r.y },     d = { x + r.height, r.y };
        DrawTriangle(a, b, c, color);
        DrawTriangle(a, c, d, color);
    }
    EndScissorMode();
}

void Dim(float alpha) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(BLACK, alpha));
}

// ═════════════════════════════════════════════════════════════════════════════
// Interaction & navigation clavier
// ═════════════════════════════════════════════════════════════════════════════

static bool              s_inputEnabled = true;
static void            (*s_clickCallback)(void*) = nullptr;
static void*             s_clickUser = nullptr;

void SetClickSound(void (*callback)(void* user), void* user) {
    s_clickCallback = callback;
    s_clickUser     = user;
}

static void PlayClick() { if (s_clickCallback) s_clickCallback(s_clickUser); }
static int               s_focus        = -1;
static int               s_count        = 0;
static bool              s_keyboard     = false;
static bool              s_activate     = false;
static int               s_step         = 0;
static std::vector<bool> s_enabled, s_prevEnabled;

void BeginFrame() {
    s_prevEnabled.swap(s_enabled);
    s_enabled.clear();
    s_count    = 0;
    s_activate = false;
    s_step     = 0;

    int n = (int)s_prevEnabled.size();
    if (n == 0) { s_focus = -1; return; }
    if (s_focus >= n) s_focus = n - 1;

    Vector2 md = GetMouseDelta();
    if (md.x != 0.0f || md.y != 0.0f) s_keyboard = false;

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int dir = 0;
    if (IsKeyPressed(KEY_DOWN) || (IsKeyPressed(KEY_TAB) && !shift)) dir = 1;
    if (IsKeyPressed(KEY_UP)   || (IsKeyPressed(KEY_TAB) &&  shift)) dir = -1;

    if (dir != 0) {
        int f = s_keyboard ? s_focus : -1;
        s_keyboard = true;
        for (int i = 0; i < n; i++) {
            f = (f < 0) ? (dir > 0 ? 0 : n - 1) : (f + dir + n) % n;
            if (s_prevEnabled[f]) break;
        }
        s_focus = f;
    }

    if (s_keyboard && s_focus >= 0) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) s_activate = true;
        if (IsKeyPressed(KEY_LEFT))  s_step = -1;
        if (IsKeyPressed(KEY_RIGHT)) s_step = 1;
    }
}

void ResetFocus() {
    s_focus    = -1;
    s_keyboard = false;
}

void SetInputEnabled(bool enabled) { s_inputEnabled = enabled; }

bool Hover(Rectangle r) { return s_inputEnabled && CheckCollisionPointRec(GetMousePosition(), r); }

bool Clicked(Rectangle r, int button) { return IsMouseButtonPressed(button) && Hover(r); }

namespace {

struct NavState { bool hover, focused, down; };

// Enregistre un élément navigable et renvoie son état
NavState Register(Rectangle r, bool enabled) {
    NavState st{ false, false, false };
    if (!s_inputEnabled) return st;

    int idx = s_count++;
    s_enabled.push_back(enabled);

    st.hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    if (st.hover && !s_keyboard) s_focus = idx;
    st.focused = enabled && idx == s_focus && (s_keyboard || st.hover);
    st.down    = st.hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    return st;
}

bool DrawButton(Rectangle r, const char* label, bool enabled, Color accent, float size, bool mono) {
    NavState st = Register(r, enabled);

    float off = st.down ? 2.0f : 0.0f;
    if (enabled && !st.down)
        DrawRectangleRec({ r.x + 3, r.y + 3, r.width, r.height }, ColorAlpha(BLACK, 0.35f));

    Rectangle b  = { r.x + off, r.y + off, r.width, r.height };
    Color bg     = st.focused ? accent : ColorAlpha(C_INK, 0.55f);
    Color fg     = !enabled ? C_TEXT_DIM : st.focused ? C_INK : C_TEXT;
    Color border = !enabled ? ColorAlpha(C_BORDER, 0.6f) : st.focused ? accent : ColorAlpha(accent, 0.6f);

    DrawRectangleRec(b, bg);
    DrawRectangleLinesEx(b, st.focused ? 2.0f : 1.0f, border);

    if (mono) {
        float ty = b.y + (b.height - size) * 0.5f;
        if (st.focused) Text(">", b.x + 10, ty, size, fg);
        Text(label, b.x + 30, ty, size, fg);
    } else {
        float ty = b.y + (b.height - SnapPixel(size)) * 0.5f;
        if (st.focused) {
            std::string l = std::string("> ") + label + " <";
            LabelCentered(l.c_str(), b.x + b.width * 0.5f, ty, size, fg);
        } else {
            LabelCentered(label, b.x + b.width * 0.5f, ty, size, fg);
        }
    }

    bool clicked = enabled && ((st.hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
                               (st.focused && s_activate));
    if (clicked) { s_activate = false; PlayClick(); }
    return clicked;
}

} // namespace

bool Button(Rectangle r, const char* label, bool enabled, Color accent, float size) {
    return DrawButton(r, label, enabled, accent, size, false);
}

bool ButtonText(Rectangle r, const char* label, bool enabled, Color accent, float size) {
    return DrawButton(r, label, enabled, accent, size, true);
}

int Stepper(Rectangle r, const char* label, const char* value, Color accent) {
    NavState st = Register(r, true);

    DrawRectangleRec(r, st.focused ? ColorAlpha(accent, 0.15f) : ColorAlpha(C_INK, 0.4f));
    DrawRectangleLinesEx(r, 1.0f, st.focused ? accent : ColorAlpha(C_BORDER, 0.7f));
    if (st.focused) DrawRectangleRec({ r.x, r.y, 6, r.height }, accent);

    float ty = r.y + (r.height - 20.0f) * 0.5f;
    Label(label, r.x + 20, ty, 20, st.focused ? C_TEXT : C_LABEL);

    const float arrowsW = 260.0f;
    float vx = r.x + r.width - arrowsW;
    Rectangle left  = { vx, r.y, 40, r.height };
    Rectangle right = { r.x + r.width - 40, r.y, 40, r.height };
    bool hl = s_inputEnabled && CheckCollisionPointRec(GetMousePosition(), left);
    bool hr = s_inputEnabled && CheckCollisionPointRec(GetMousePosition(), right);

    LabelCentered("<", left.x + 20,  ty, 20, hl ? accent : C_LABEL);
    LabelCentered(">", right.x + 20, ty, 20, hr ? accent : C_LABEL);
    LabelCentered(value, vx + arrowsW * 0.5f, ty, 20, st.focused ? accent : C_TEXT);

    int result = 0;
    if (s_inputEnabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hl)                  result = -1;
        else if (hr || st.hover) result = 1;
    }
    if (st.focused && s_keyboard) {
        if (s_step != 0)     result = s_step;
        else if (s_activate) { result = 1; s_activate = false; }
    }
    if (result != 0) PlayClick();
    return result;
}

} // namespace ui
