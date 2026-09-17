#pragma once
#include "raylib.h"
#include "Profile.hpp"
#include "SaveGame.hpp"
#include "../audio/AudioManager.hpp"
#include "../combat/Combat.hpp"
#include "../core/Settings.hpp"
#include "../effect/Effects.hpp"
#include "../effect/PostFX.hpp"
#include "../effect/SpaceBackground.hpp"
#include "../effect/Starfield.hpp"
#include "../event/Events.hpp"
#include "../hud/HUD.hpp"
#include "../map/SectorMap.hpp"
#include "../projectile/ProjectileManager.hpp"
#include "../ship/Ship.hpp"
#include "../ship/ShipRenderer.hpp"
#include "../shop/ShipMenu.hpp"
#include "../shop/Store.hpp"
#include <functional>
#include <string>
#include <vector>

enum class GameState {
    MENU,
    BEACON,     // à l'arrêt sur une balise
    MAP,
    JUMPING,
    EVENT,
    COMBAT,
    STORE,
    GAME_OVER,
    VICTORY,
};

class Game {
public:
    void Init();
    void Run();
    void Shutdown();

private:
    // ── Services ─────────────────────────────────────────────────────────────
    Settings          _settings;
    AudioManager      _audio;
    ProjectileManager _proj;
    Effects           _fx;
    Starfield         _stars;
    SpaceBackground   _bg;
    PostFX            _post;
    ShipRenderer      _renderer;
    HUD               _hud;
    RenderTexture2D   _canvas{};

    SectorMap _map;
    Store     _store;
    ShipMenu  _shipMenu;
    Combat    _combat;

    // ── Partie ───────────────────────────────────────────────────────────────
    Ship      _player;
    ShipView  _playerView;
    GameState _state     = GameState::MENU;
    GameState _prevState = GameState::MENU;
    bool      _quit      = false;
    float     _t         = 0.0f;
    float     _ftl       = 0.0f;
    float     _bootT     = 0.0f;

    // Surcouches
    bool      _pause        = false;
    bool      _help         = false;
    bool      _settingsOpen = false;
    bool      _confirmNew   = false;
    bool      _setupOpen    = false;
    bool      _archivesOpen = false;

    // Progression et choix de partie
    Profile          _profile;
    int              _selectedShip = SHIP_AUGATON;
    int              _difficulty   = 1;
    std::vector<int> _newUnlocks;

    // Tutoriel du premier combat
    int   _tutStep  = 0;
    float _tutTimer = 0.0f;
    bool  _tutPausedOnce = false;
    void  _UpdateTutorial(float dt);
    void  _DrawTutorial();
    int       _layerKey     = -1;

    RunStats  _stats;
    int       _lastScrap = 0;
    int       _prevHull  = 0;

    // Sauvegarde
    bool        _hasSave = false;
    std::string _saveError;

    // Saut
    float _jumpT          = 0.0f;
    int   _jumpTarget     = -1;
    bool  _jumpNextSector = false;
    bool  _jumpSwapped    = false;
    bool  _jumpRevisit    = false;

    EnemyKind _combatKind = EnemyKind::NORMAL;

    // Événement en cours
    const EventDef*          _event = nullptr;
    std::string              _eventTitle;
    std::string              _eventBody;
    bool                     _eventResolved = false;
    std::string              _resultText;
    std::vector<std::string> _resultLines;
    Outcome                  _pendingOutcome;
    float                    _typed        = 0.0f;   // caractères révélés
    bool                     _justRevealed = false;
    float                    _eventAnim    = 1.0f;

    std::string              _endReason;
    std::string              _toast;
    float                    _toastT = 0.0f;

    // Actions différées (boutons cliqués pendant le rendu)
    std::vector<std::function<void()>> _deferred;
    void _Defer(std::function<void()> fn) { _deferred.push_back(std::move(fn)); }

    // ── Boucle ───────────────────────────────────────────────────────────────
    void _Frame();
    void _Update(float dt);
    void _Render();
    void _UpdateMouseMapping() const;
    void _HandleEscape();
    bool _InRun() const;
    bool _OverlayOpen() const {
        return _pause || _help || _settingsOpen || _confirmNew || _shipMenu.IsOpen() || _setupOpen || _archivesOpen;
    }

    // ── Paramètres & fichiers ────────────────────────────────────────────────
    void _ApplySettings();
    void _Screenshot();
    void _RefreshSaveStatus();
    void _SaveRun(ResumePoint point, EnemyKind kind = EnemyKind::NORMAL);
    void _ContinueRun();

    // ── Flux de jeu ──────────────────────────────────────────────────────────
    void _NewGame();
    void _ReturnToMenu();
    void _ApplyBackground();
    void _BeginJump(int target);
    void _BeginSectorJump();
    void _UpdateJump(float dt);
    void _ArriveAtBeacon(bool revisit);
    void _StartEvent(const EventDef& def);
    void _ChooseOption(int index);
    void _ShowResult(const std::string& title, const std::string& text,
                     const std::vector<std::string>& lines);
    void _ContinueAfterEvent();
    void _ContinueToMap();
    bool _OutcomeIsPlain() const;
    void _StartCombat(EnemyKind kind);
    void _EndCombat(CombatResult result);
    void _Distress();
    void _GameOver(const std::string& reason);
    void _Victory(const std::string& reason);

    // ── Rendu des écrans ─────────────────────────────────────────────────────
    void _DrawMenu();
    void _DrawBeaconUI();
    void _DrawEvent();
    void _DrawJump();
    void _DrawEndScreen(bool victory);
    void _DrawPauseMenu();
    void _DrawSettings();
    void _DrawConfirm();
    void _DrawSetup();
    void _DrawArchives();
    void _LaunchFromSetup();
    void _RecordRun(bool won);
    void _DrawHelp();
    void _DrawToast();
    int   _EventTextLength() const;
    float _TypeSpeed() const;
    Vector2 _PlayerHome() const;
};
