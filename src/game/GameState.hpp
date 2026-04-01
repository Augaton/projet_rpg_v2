#pragma once

// ─── État global du jeu ───────────────────────────────────────────────────────

enum class GameState {
    IDLE,        // Aucun combat — écran centré sur le joueur
    COMBAT,      // Combat en cours — écran splitté
    EVENT_SHIP,  // Événement avec vaisseau (marchand…) — écran splitté
    EVENT_NONE   // Événement sans vaisseau (texte, loot…) — écran centré
};

// ─── Type d'événement rencontré à un nœud ────────────────────────────────────

enum class EventType {
    NONE,
    COMBAT,
    MERCHANT,
    DERELICT    // Épave : loot aléatoire
};
