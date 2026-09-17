#include "Game.hpp"
#include "../audio/Synth.hpp"
#include "../core/Config.hpp"
#include "../core/Paths.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include "../ship/ShipFactory.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

using namespace ui;

static constexpr float JUMP_DURATION = 2.2f;
static constexpr float JUMP_SWAP     = 1.1f;
static constexpr const char* VERSION_TAG = "v2.1";

// ═════════════════════════════════════════════════════════════════════════════
// Initialisation
// ═════════════════════════════════════════════════════════════════════════════

void Game::Init() {
    _settings.Load();
    _profile.Load();

    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT;
    if (_settings.vsync) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(VIRTUAL_W, VIRTUAL_H, "Augaton");
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);

    _canvas = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(_canvas.texture, TEXTURE_FILTER_BILINEAR);

    ui::LoadFont();
    _post.Load();
    _bg.Load("src/ressources/shader/nebula.fs");
    _bg.SetLevels(10.0f);

    _stars.Init(220, VIRTUAL_W, VIRTUAL_H);
    _renderer.Load();
    _proj.Load();

    _audio.Init();
    _audio.LoadMusic("ambience", "asset/Sound/ambience/normal.ogg");
    _audio.LoadMusic("engine",   "asset/Sound/sfx/engine/engine_idle.ogg");
    _audio.LoadSfx("jump",    "asset/Sound/sfx/ftl/ftl_boom.wav", 2);
    _audio.LoadSfx("laser",   "asset/Sound/sfx/weapons/laser_fire.wav", 6);
    _audio.LoadSfx("missile", "asset/Sound/sfx/weapons/missile_fire.wav", 6);
    for (int i = 1; i <= 5; i++)
        _audio.LoadSfx("gun_" + std::to_string(i), "asset/Sound/sfx/weapons/gun_" + std::to_string(i) + ".wav", 3);
    RegisterSynthSounds(_audio);
    _audio.PlayMusic("ambience");
    _audio.PlayMusic("engine");
    _hud.SetAudio(&_audio);
    ui::SetClickSound([](void* user) { static_cast<AudioManager*>(user)->PlaySfx("sx_click", 1.0f, 0.45f); }, &_audio);

    _combat.Init(&_audio, &_proj, &_fx, &_renderer, &_hud);
    _ApplySettings();

    _player = MakePlayerShip();
    _playerView.pos = _PlayerHome();
    _map.Generate(0, false);
    _ApplyBackground();
    _RefreshSaveStatus();
}

void Game::Run() {
    while (!WindowShouldClose() && !_quit) _Frame();
}

void Game::Shutdown() {
    _proj.Unload();
    _renderer.Unload();
    _bg.Unload();
    _post.Unload();
    UnloadRenderTexture(_canvas);
    ui::UnloadFont();
    _audio.Close();
    CloseWindow();
}

// ═════════════════════════════════════════════════════════════════════════════
// Paramètres & fichiers
// ═════════════════════════════════════════════════════════════════════════════

void Game::_ApplySettings() {
    _audio.SetVolumes(_settings.masterVolume, _settings.musicVolume, _settings.sfxVolume);

    bool borderless = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (borderless != _settings.fullscreen) ToggleBorderlessWindowed();

    if (_settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else                 ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(_settings.fpsCap);

    _fx.SetIntensity(_settings.shake, _settings.reduceFlashes ? 0.25f : 1.0f);
    _combat.SetAutoPause(_settings.autoPause);
}

void Game::_Screenshot() {
    Image img = LoadImageFromTexture(_canvas.texture);
    ImageFlipVertical(&img);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (img.data) {
        auto* px = (unsigned char*)img.data;
        for (int i = 0, n = img.width * img.height; i < n; i++) px[i * 4 + 3] = 255;   // image opaque
    }

    char stamp[32] = "capture";
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);

    std::string path = paths::ScreenshotDir() + "/augaton_" + stamp + ".png";
    _toast  = ExportImage(img, path.c_str()) ? "Capture enregistrée : " + path : "Échec de la capture d'écran";
    _toastT = 3.0f;
    UnloadImage(img);
}

void Game::_RefreshSaveStatus() {
    _saveError.clear();
    _hasSave = false;
    if (!save::Exists()) return;

    SaveData probe;
    std::string err;
    _hasSave = save::Read(probe, err);
    if (!_hasSave) _saveError = err;
}

void Game::_SaveRun(ResumePoint point, EnemyKind kind) {
    SaveData d;
    d.map        = _map.Export();
    d.ship       = _player;
    d.stats      = _stats;
    d.resume     = point;
    d.combatKind = kind;
    d.difficulty = _difficulty;
    _hasSave = save::Write(d);
    if (!_hasSave) {
        _toast  = "Impossible d'écrire la sauvegarde";
        _toastT = 3.0f;
    }
}

void Game::_ContinueRun() {
    SaveData d;
    std::string err;
    if (!save::Read(d, err) || !_map.Import(d.map)) {
        _saveError = err.empty() ? "carte invalide" : err;
        _hasSave   = false;
        return;
    }

    _player     = d.ship;
    _difficulty = d.difficulty;
    _selectedShip = d.ship.shipType;
    _stats      = d.stats;
    _playerView = ShipView{};
    _playerView.pos = { -220.0f, 350.0f };
    _lastScrap  = _player.scrap;
    _prevHull   = _player.hull;

    _proj.Clear();
    _fx.Clear();
    _hud.ClearLog();
    _hud.ResetVisuals();
    _ApplyBackground();
    _hud.Log("Partie reprise", C_ACCENT);

    _state     = GameState::BEACON;
    _prevState = GameState::BEACON;   // pas de nouvelle sauvegarde immédiate

    switch (d.resume) {
    case ResumePoint::ARRIVAL: _ArriveAtBeacon(false);      break;
    case ResumePoint::COMBAT:  _StartCombat(d.combatKind);  break;
    default:                                                break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Boucle
// ═════════════════════════════════════════════════════════════════════════════

void Game::_UpdateMouseMapping() const {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float scale = std::min(sw / VIRTUAL_W, sh / VIRTUAL_H);
    if (scale <= 0.0f) return;
    float ox = (sw - VIRTUAL_W * scale) * 0.5f;
    float oy = (sh - VIRTUAL_H * scale) * 0.5f;
    SetMouseOffset((int)-ox, (int)-oy);
    SetMouseScale(1.0f / scale, 1.0f / scale);
}

bool Game::_InRun() const {
    return _state != GameState::MENU && _state != GameState::GAME_OVER && _state != GameState::VICTORY;
}

void Game::_Frame() {
    // Fenêtre réduite : on ne rend rien et on laisse le CPU tranquille
    if (IsWindowMinimized()) {
        PollInputEvents();
        _audio.Update();
        WaitTime(0.05);
        return;
    }

    float dt = std::min(GetFrameTime(), 0.05f);
    _t += dt;

    if (IsKeyPressed(KEY_F11) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
        _settings.fullscreen = !_settings.fullscreen;
        _ApplySettings();
        _settings.Save();
    }

    // Pause automatique quand la fenêtre perd le focus
    if (_settings.pauseOnFocus && !IsWindowFocused() && _InRun() && !_pause && _state != GameState::JUMPING)
        _pause = true;

    _UpdateMouseMapping();
    ui::BeginFrame();
    _audio.Update();
    _Update(dt);
    _Render();

    if (IsKeyPressed(KEY_F12)) _Screenshot();
}

Vector2 Game::_PlayerHome() const {
    switch (_state) {
    case GameState::MENU:   return { 900.0f, 390.0f };
    case GameState::COMBAT: return { 330.0f, 360.0f };
    default:                return { 470.0f, 350.0f };
    }
}

void Game::_HandleEscape() {
    if (_help)         { _help = false; return; }
    if (_shipMenu.IsOpen()) { _shipMenu.Close(); return; }
    if (_settingsOpen) { _settingsOpen = false; _settings.Save(); return; }
    if (_confirmNew)   { _confirmNew = false; return; }
    if (_setupOpen)    { _setupOpen = false; return; }
    if (_archivesOpen) { _archivesOpen = false; return; }

    switch (_state) {
    case GameState::MENU:
    case GameState::GAME_OVER:
    case GameState::VICTORY:
        return;
    case GameState::MAP:
        if (!_pause && _map.IsClosable()) { _map.Close(); _state = GameState::BEACON; return; }
        break;
    case GameState::STORE:
        if (!_pause) { _store.Close(); _state = GameState::BEACON; return; }
        break;
    case GameState::COMBAT:
        if (!_pause && _combat.CancelTargeting()) return;
        break;
    default:
        break;
    }
    _pause = !_pause;
}

float Game::_TypeSpeed() const {
    switch (_settings.textSpeed) {
    case 0:  return 1.0e6f;
    case 2:  return 260.0f;
    default: return 110.0f;
    }
}

int Game::_EventTextLength() const {
    return ui::CharCount(_eventResolved ? _resultText.c_str() : _eventBody.c_str());
}

void Game::_Update(float dt) {
    // Actions déclenchées par des boutons au frame précédent
    auto deferred = std::move(_deferred);
    _deferred.clear();
    for (auto& fn : deferred) fn();

    if (IsKeyPressed(KEY_ESCAPE)) _HandleEscape();
    if (IsKeyPressed(KEY_F1))     _help = !_help;

    // Menu VAISSEAU : à l'arrêt sur une balise
    if (IsKeyPressed(KEY_U) && _state == GameState::BEACON && !_pause && !_help && !_settingsOpen) {
        if (_shipMenu.IsOpen()) _shipMenu.Close();
        else                    _shipMenu.Open();
    }
    if (_shipMenu.CloseRequested()) _shipMenu.Close();
    if (_state != GameState::BEACON && _shipMenu.IsOpen()) _shipMenu.Close();
    _map.SetSensorLevel(_player.sys[SYS_SENSORS].level);

    // Réinitialise le focus clavier quand l'écran actif change
    int layer = (int)_state * 512 + (_pause ? 1 : 0) + (_help ? 2 : 0) + (_settingsOpen ? 4 : 0)
              + (_confirmNew ? 8 : 0) + (_eventResolved ? 16 : 0) + (_shipMenu.IsOpen() ? 32 : 0)
              + (_setupOpen ? 128 : 0) + (_archivesOpen ? 256 : 0);
    if (layer != _layerKey) { ui::ResetFocus(); _layerKey = layer; }

    // Ambiance (continue même en pause)
    float ftlTarget = (_state == GameState::JUMPING && _jumpT < JUMP_SWAP + 0.4f) ? 1.0f : 0.0f;
    _ftl += (ftlTarget - _ftl) * std::min(1.0f, 2.5f * dt);

    bool impacted = _player.hull < _prevHull;
    _prevHull = _player.hull;
    _bg.Update(dt, _ftl, impacted && !_settings.reduceFlashes);

    _audio.SetMusicPitch("engine",    1.0f + _ftl * 0.4f);
    _audio.SetMusicVolume("engine",   0.2f + _ftl * 0.3f);
    _audio.SetMusicVolume("ambience", 0.4f - _ftl * 0.2f);

    _bootT  += dt;
    _toastT  = std::max(0.0f, _toastT - dt);
    _stars.Update(dt, _OverlayOpen() ? 0.0f : _ftl);

    if (_OverlayOpen()) return;

    // En combat, les effets suivent la vitesse de jeu (×2, ×4, ralenti, gel d'impact)
    float fxScale = (_state == GameState::COMBAT) ? _combat.TimeScale() : 1.0f;
    _fx.Update(dt * fxScale, _ftl * 1500.0f);


    if (!(_state == GameState::JUMPING && _jumpT < JUMP_SWAP)) {
        Vector2 home = _PlayerHome();
        home.y += std::sin(_t * 0.9f) * 5.0f;
        float k = std::min(1.0f, dt * 2.2f);
        _playerView.pos.x += (home.x - _playerView.pos.x) * k;
        _playerView.pos.y += (home.y - _playerView.pos.y) * k;
    }

    if (_InRun()) _hud.Update(dt, _player);

    if (_player.scrap > _lastScrap) _stats.scrap += _player.scrap - _lastScrap;
    _lastScrap = _player.scrap;

    switch (_state) {
    case GameState::BEACON:
        _player.Update(dt, false);
        _hud.HandleInput(_player, nullptr, false);
        if (IsKeyPressed(KEY_J) || ui::Clicked(HUD::JumpButtonRect())) {
            _map.Open(true);
            _state = GameState::MAP;
        }
        break;

    case GameState::MAP: {
        _player.Update(dt, false);
        int target = _map.Update(dt, _player.fuel);
        if (target >= 0)                    _BeginJump(target);
        else if (_map.DistressRequested())  _Distress();
        else if (_map.CloseRequested())     { _map.Close(); _state = GameState::BEACON; }
        break;
    }

    case GameState::JUMPING:
        _player.Update(dt, false);
        // Saut passable : clic / espace / entrée accélèrent jusqu'à l'étape suivante
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (!_jumpSwapped) _jumpT = std::max(_jumpT, JUMP_SWAP);
            else               _jumpT = std::max(_jumpT, JUMP_DURATION - 0.15f);
        }
        _UpdateJump(dt);
        break;

    case GameState::EVENT: {
        _player.Update(dt, false);
        _eventAnim = std::min(1.0f, _eventAnim + dt * 6.0f);
        _justRevealed = false;
        int len = _EventTextLength();
        if (_typed < (float)len) {
            _typed += _TypeSpeed() * dt;
            bool skip = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE) ||
                        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
            if (skip) {
                _typed = (float)len;
                _justRevealed = true;
            }
        } else if (!_eventResolved && _event) {
            for (int i = 0; i < (int)_event->choices.size() && i < 9; i++)
                if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i)) { _ChooseOption(i); break; }
        } else if (_eventResolved && IsKeyPressed(KEY_SPACE)) {
            _ContinueAfterEvent();
        } else if (_eventResolved && IsKeyPressed(KEY_J) && _OutcomeIsPlain()) {
            _ContinueToMap();
        }
        break;
    }

    case GameState::COMBAT: {
        bool consumed = _hud.HandleInput(_player, _combat.SelectedWeapon(), true);

        if (!_combat.Dying() && _player.ftlCharge >= 1.0f &&
            (IsKeyPressed(KEY_J) || ui::Clicked(HUD::JumpButtonRect()))) {
            _combat.End();
            _player.RepairSystems();
            _hud.Log("Saut d'urgence : choisissez une destination !", C_SHIELD);
            _map.Open(false);
            _state = GameState::MAP;
            break;
        }

        CombatResult r = _combat.Update(dt, consumed);
        _UpdateTutorial(dt);
        if (r != CombatResult::ONGOING) _EndCombat(r);
        break;
    }

    case GameState::STORE:
        _player.Update(dt, false);
        if (_store.CloseRequested()) { _store.Close(); _state = GameState::BEACON; }
        break;

    case GameState::VICTORY:
        if (rng::Chance(dt * 2.5f)) {
            static const Color colors[] = { C_SHIELD, C_GOOD, C_SCRAP, C_ION, C_WARNING };
            Vector2 p = { rng::Float(150.0f, 1130.0f), rng::Float(80.0f, 300.0f) };
            _fx.Spark(p, colors[rng::Int(0, 4)], 50, 260.0f, 3.0f, 1.3f);
        }
        break;

    default:
        break;
    }

    // Sauvegarde automatique à chaque arrêt sur une balise
    if (_state == GameState::BEACON && _prevState != GameState::BEACON) _SaveRun(ResumePoint::BEACON);
    _prevState = _state;
}

// ═════════════════════════════════════════════════════════════════════════════
// Flux de jeu
// ═════════════════════════════════════════════════════════════════════════════

void Game::_NewGame() {
    if (_selectedShip < 0 || _selectedShip >= PLAYER_SHIP_COUNT || !_profile.unlocked[_selectedShip])
        _selectedShip = SHIP_AUGATON;
    _player     = MakePlayerShip(_selectedShip);
    _newUnlocks.clear();
    _playerView = ShipView{};
    _playerView.pos = { -220.0f, 350.0f };

    _stats     = {};
    _lastScrap = _player.scrap;
    _prevHull  = _player.hull;
    _pause = _help = _settingsOpen = _confirmNew = _setupOpen = _archivesOpen = false;

    _proj.Clear();
    _fx.Clear();
    _hud.ClearLog();
    _hud.ResetVisuals();

    _map.Generate(0, SECTOR_COUNT == 1);
    _ApplyBackground();

    _state     = GameState::BEACON;
    _prevState = GameState::BEACON;
    _SaveRun(ResumePoint::BEACON);
    _StartEvent(IntroEvent());
}

void Game::_ReturnToMenu() {
    _pause = _help = _settingsOpen = _confirmNew = false;
    _combat.End();
    _map.Close();
    _store.Close();
    _player     = MakePlayerShip();
    _playerView = ShipView{};
    _playerView.pos = { 900.0f, 390.0f };
    _fx.Clear();
    _state = GameState::MENU;
    _bootT = 0.0f;
    _map.Generate(0, false);
    _ApplyBackground();
    _RefreshSaveStatus();
}

void Game::_ApplyBackground() {
    const SectorDef& s = SECTORS[_map.Sector()];
    Color c1 = s.c1, c2 = s.c2;
    if (_map.Current().kind == BeaconKind::NEBULA) {
        c1 = { 140, 70, 210, 255 };
        c2 = {  40, 20,  90, 255 };
    }
    switch (_map.Current().hazard) {
    case BeaconHazard::ASTEROIDS:   c1 = { 120, 110, 100, 255 }; c2 = {  40,  36,  34, 255 }; break;
    case BeaconHazard::SOLAR_FLARE: c1 = { 255, 140,  40, 255 }; c2 = {  90,  30,  10, 255 }; break;
    case BeaconHazard::ION_STORM:   c1 = {  90,  90, 255, 255 }; c2 = { 120,  40, 200, 255 }; break;
    default: break;
    }
    _bg.SetSector(s.seed + (float)_map.CurrentIdx() * 1.37f, c1, c2);
}

void Game::_BeginJump(int target) {
    _player.fuel    = std::max(0, _player.fuel - 1);
    _jumpTarget     = target;
    _jumpNextSector = false;
    _jumpT          = 0.0f;
    _jumpSwapped    = false;
    _jumpRevisit    = false;
    _stats.jumps++;

    _map.Close();
    _map.ClearRequests();
    _state = GameState::JUMPING;
    _audio.PlaySfx("jump");
}

void Game::_BeginSectorJump() {
    int fuel = _player.fuel;
    _BeginJump(-1);
    _player.fuel    = fuel;          // le relais de sortie est gratuit
    _jumpNextSector = true;
}

void Game::_UpdateJump(float dt) {
    _jumpT += dt;

    if (_jumpT < JUMP_SWAP) {
        float k = _jumpT / JUMP_SWAP;
        _playerView.pos.x += (40.0f + 1600.0f * k * k) * dt;
    }

    if (!_jumpSwapped && _jumpT >= JUMP_SWAP) {
        _jumpSwapped = true;

        if (_jumpNextSector) {
            int next = std::min(_map.Sector() + 1, SECTOR_COUNT - 1);
            _map.Generate(next, next == SECTOR_COUNT - 1);
            _hud.Log(TextFormat("Secteur %d : %s", next + 1, SECTORS[next].name), C_WARNING);
        } else {
            bool fromNebula = _map.Current().kind == BeaconKind::NEBULA;
            _jumpRevisit    = _map.JumpTo(_jumpTarget);
            bool toNebula   = _map.Current().kind == BeaconKind::NEBULA;
            float fleet = GetDifficulty(_difficulty).fleet;
            _map.AdvanceFleet((fromNebula || toNebula) ? fleet * 0.56f : fleet);
        }

        _ApplyBackground();
        _playerView.pos.x = -220.0f;
        if (!_jumpRevisit) _SaveRun(ResumePoint::ARRIVAL);
    }

    if (_jumpT >= JUMP_DURATION) {
        _fx.Flash(0.5f);
        _fx.Shake(6.0f);
        _state = GameState::BEACON;
        _prevState = GameState::BEACON;   // l'événement d'arrivée décide de la suite
        _ArriveAtBeacon(_jumpRevisit);
    }
}

void Game::_ArriveAtBeacon(bool revisit) {
    const Beacon& b = _map.Current();

    if (b.kind != BeaconKind::BOSS && _map.IsOvertaken(_map.CurrentIdx())) {
        _StartEvent(FleetEvent());
        return;
    }
    if (revisit) {
        // Les balises déjà visitées peuvent être reprises par une patrouille
        bool special = b.kind == BeaconKind::STORE || b.kind == BeaconKind::EXIT || b.kind == BeaconKind::BOSS;
        if (!special && rng::Chance(0.35f)) _StartEvent(PatrolEvent());
        else                                _StartEvent(RevisitEvent(b.kind));
        return;
    }
    _stats.events++;
    _StartEvent(PickEvent(b.kind));
}

void Game::_StartEvent(const EventDef& def) {
    _event          = &def;
    _eventTitle     = def.title;
    _eventBody      = def.body;
    _eventResolved  = false;
    _resultText.clear();
    _resultLines.clear();
    _pendingOutcome = Outcome{};
    _typed          = 0.0f;
    _eventAnim      = 0.0f;
    _state          = GameState::EVENT;
}

void Game::_ChooseOption(int index) {
    if (!_event || _eventResolved || index < 0 || index >= (int)_event->choices.size()) return;

    const Choice& c = _event->choices[index];
    if (!ChoiceAvailable(c, _player)) return;

    const Outcome& o = rng::Chance(c.chance) ? c.success : c.failure;
    _pendingOutcome  = o;
    _resultLines     = ApplyOutcome(o, _player);

    if (o.fleet != 0.0f) {
        _map.AdvanceFleet(o.fleet);
        _resultLines.push_back(o.fleet < 0.0f ? "La flotte rebelle perd votre trace" : "La flotte rebelle se rapproche");
    }
    for (const auto& line : _resultLines)
        if (!line.empty()) _hud.Log(line, line[0] == '-' ? C_HULL_LOW : C_GOOD);

    _resultText    = o.text ? o.text : "";
    _eventResolved = true;
    _typed         = 0.0f;

    if (_resultText.empty() && _resultLines.empty()) _ContinueAfterEvent();
}

void Game::_ShowResult(const std::string& title, const std::string& text,
                       const std::vector<std::string>& lines) {
    _event          = nullptr;
    _eventTitle     = title;
    _eventBody.clear();
    _eventResolved  = true;
    _resultText     = text;
    _resultLines    = lines;
    _pendingOutcome = Outcome{};
    _typed          = 0.0f;
    _eventAnim      = 0.0f;
    _state          = GameState::EVENT;

    for (const auto& line : lines)
        if (!line.empty()) _hud.Log(line, line[0] == '-' ? C_HULL_LOW : C_GOOD);
}

bool Game::_OutcomeIsPlain() const {
    return !_pendingOutcome.combat && !_pendingOutcome.store && !_pendingOutcome.nextSector;
}

// Raccourci : termine l'événement et ouvre directement la carte (en sauvegardant la balise)
void Game::_ContinueToMap() {
    _ContinueAfterEvent();
    if (_state != GameState::BEACON) return;
    _SaveRun(ResumePoint::BEACON);
    _prevState = GameState::MAP;
    _map.Open(true);
    _state = GameState::MAP;
}

void Game::_ContinueAfterEvent() {
    Outcome o = _pendingOutcome;
    _pendingOutcome = Outcome{};
    _event = nullptr;

    if (o.combat) {
        _StartCombat(o.enemy);
    } else if (o.store) {
        _store.Generate(_map.Sector(), _map.Sector() * 100 + _map.CurrentIdx());
        _store.Open();
        _state = GameState::STORE;
    } else if (o.nextSector) {
        _BeginSectorJump();
    } else {
        _state = GameState::BEACON;
    }
}

void Game::_StartCombat(EnemyKind kind) {
    _SaveRun(ResumePoint::COMBAT, kind);

    Ship enemy  = MakeEnemyShip(_map.Sector(), kind);
    const DifficultyDef& diff = GetDifficulty(_difficulty);
    enemy.hull       = std::max(1, (int)std::lround((float)enemy.hull * diff.enemyHull));
    enemy.maxHull    = enemy.hull;
    enemy.chargeMult *= diff.enemyCharge;
    _combatKind = kind;
    _combat.Begin(&_player, &_playerView, enemy, kind, _map.Current().hazard);
    if (enemy.trait >= 0) _hud.Log(TextFormat("ÉLITE : %s", EliteTraitName(enemy.trait)), C_SCRAP);
    if (_map.Current().hazard != BeaconHazard::NONE) _hud.Log(TextFormat("Danger : %s", HazardName(_map.Current().hazard)), C_WARNING);
    _state = GameState::COMBAT;

    _hud.Log("Contact hostile : " + enemy.name, C_ENEMY);
    _hud.Log("[ESPACE] pause tactique", C_TEXT_DIM);
    _audio.PlaySfx("jump", 1.6f, 0.35f);
}

void Game::_EndCombat(CombatResult result) {
    std::string enemyName = _combat.Enemy().name;
    bool boss  = _combat.IsBoss();
    bool elite = _combat.Enemy().trait >= 0;
    _combat.End();

    switch (result) {
    case CombatResult::ENEMY_DESTROYED: {
        _stats.kills++;
        _player.RepairSystems();
        if (boss) {
            _Victory("Le vaisseau amiral rebelle se disloque dans un torrent de flammes. "
                     "La flotte est décapitée : les données sont en sécurité.");
            return;
        }

        int sector = _map.Sector();
        Outcome reward;
        if (_combatKind == EnemyKind::FLEET) {
            reward.scrap = rng::Int(4, 10);
        } else {
            reward.scrap    = 12 + sector * 6 + rng::Int(0, 10);
            reward.fuel     = rng::Int(0, 2) + (rng::Chance(0.25f) ? 1 : 0);
            reward.missiles = rng::Int(1, 3);
            if (rng::Chance(0.12f)) reward.weapon = RANDOM_WEAPON;
        }
        float mult = GetDifficulty(_difficulty).reward * (_player.HasAug(AUG_SCRAP_ARM) ? 1.15f : 1.0f) * (elite ? 1.6f : 1.0f);
        reward.scrap = (int)std::lround((float)reward.scrap * mult);
        if (elite && rng::Chance(0.35f)) reward.augment = RANDOM_AUGMENT;
        _ShowResult(elite ? "Victoire contre une élite !" : "Victoire !",
                    enemyName + " a été détruit. Vous récupérez ce qui peut l'être parmi les débris.",
                    ApplyOutcome(reward, _player));
        break;
    }
    case CombatResult::ENEMY_FLED:
        _player.RepairSystems();
        _ShowResult("L'ennemi s'est enfui",
                    enemyName + " a sauté hors de portée avant que vous ne puissiez l'achever.", {});
        break;
    case CombatResult::PLAYER_DESTROYED:
        _GameOver("Votre vaisseau a été détruit par : " + enemyName + ".");
        break;
    default:
        break;
    }
}

void Game::_Distress() {
    _map.ClearRequests();
    _map.Close();
    _map.AdvanceFleet(1.5f);

    if (rng::Chance(0.6f)) {
        Outcome o;
        o.fuel  = rng::Int(2, 4);
        o.scrap = -std::min(_player.scrap, 8);
        _ShowResult("Signal de détresse",
                    "Après une longue attente, un marchand ambulant répond à votre appel et vous cède du carburant à prix d'ami. "
                    "Mais la flotte rebelle a gagné du terrain.",
                    ApplyOutcome(o, _player));
    } else {
        _ShowResult("Signal de détresse",
                    "Votre appel a été intercepté... par des pirates. Ils arrivent, armes chargées. La flotte rebelle a gagné du terrain.",
                    {});
        _pendingOutcome.combat = true;
    }
}

void Game::_RecordRun(bool won) {
    RunRecord r;
    r.ship       = _player.shipType;
    r.difficulty = _difficulty;
    r.sector     = _map.Sector() + 1;
    r.won        = won;
    r.kills      = _stats.kills;
    r.scrap      = _stats.scrap;
    _newUnlocks  = _profile.RecordRun(r);
    _profile.Save();
    if (!_newUnlocks.empty()) _audio.PlaySfx("sx_unlock", 1.0f, 0.7f);
}

void Game::_GameOver(const std::string& reason) {
    _RecordRun(false);
    _endReason = reason;
    _state     = GameState::GAME_OVER;
    _pause     = false;
    save::Delete();
    _hasSave = false;
}

void Game::_Victory(const std::string& reason) {
    _RecordRun(true);
    _endReason = reason;
    _state     = GameState::VICTORY;
    _fx.Flash(0.8f);
    save::Delete();
    _hasSave = false;
}

// ═════════════════════════════════════════════════════════════════════════════
// Rendu
// ═════════════════════════════════════════════════════════════════════════════

void Game::_Render() {
    // Fond procédural en basse résolution (tramé)
    if (_bg.ShouldRedraw()) {
        _post.BeginBackground();
            _bg.Draw(PostFX::BG_W, PostFX::BG_H);
        _post.EndBackground();
    }

    // Éléments lumineux pour le bloom
    _post.BeginGlow();
        _stars.Draw(_ftl);
        _proj.DrawGlow();
    _post.EndGlow();
    _post.BlurGlow();

    BeginTextureMode(_canvas);
        ClearBackground(BLACK);
        _post.DrawBackground();

        Camera2D cam{};
        cam.zoom   = 1.0f;
        cam.offset = _fx.ShakeOffset();

        BeginMode2D(cam);
            _stars.Draw(_ftl);
            _post.DrawBloom();
            if (_state == GameState::COMBAT) _combat.DrawBackdrop(_t);
            _renderer.DrawShip(_player, _playerView, _t);
            _renderer.DrawShield(_player, _playerView, _t);
            if (_state == GameState::COMBAT) _combat.DrawWorld(_t);
            _proj.Draw();
            _fx.DrawWorld();
        EndMode2D();

        if (_state == GameState::JUMPING) _DrawJump();
        _fx.DrawFlash();

        // L'écran de base ne réagit pas sous une surcouche
        ui::SetInputEnabled(!_OverlayOpen());

        switch (_state) {
        case GameState::MENU:
            _DrawMenu();
            break;
        case GameState::BEACON:
            _hud.DrawPlayer(_player, -1, false, _t);
            _hud.DrawFtlPanel(_player, false, "CARTE [J]", true);
            _DrawBeaconUI();
            _hud.DrawLog();
            break;
        case GameState::MAP:
            _hud.DrawPlayer(_player, -1, false, _t);
            _hud.DrawLog();
            _map.Draw(_player.fuel, _t);
            break;
        case GameState::JUMPING:
            _hud.DrawPlayer(_player, -1, false, _t);
            _hud.DrawLog();
            break;
        case GameState::EVENT:
            _hud.DrawPlayer(_player, -1, false, _t);
            _hud.DrawLog();
            if (_justRevealed) ui::SetInputEnabled(false);
            _DrawEvent();
            break;
        case GameState::COMBAT:
            _hud.DrawPlayer(_player, _combat.Selected(), true, _t);
            _hud.DrawFtlPanel(_player, true, "SAUT [J]", !_combat.Dying());
            _hud.DrawEnemy(_combat.Enemy(), (int)_combat.Flee(), _combat.FleeValue(), _t);
            _hud.DrawLog();
            _combat.DrawOverlay(_t);
            _DrawTutorial();
            break;
        case GameState::STORE:
            _hud.DrawPlayer(_player, -1, false, _t);
            _store.Draw(_player, _hud, _audio);
            break;
        case GameState::GAME_OVER:
            _DrawEndScreen(false);
            break;
        case GameState::VICTORY:
            _DrawEndScreen(true);
            break;
        }

        if (_shipMenu.IsOpen()) {
            ui::SetInputEnabled(!_help && !_pause && !_settingsOpen);
            _shipMenu.Draw(_player, _hud, _audio, _t);
        }
        if (_pause) {
            ui::SetInputEnabled(!_help && !_settingsOpen);
            _DrawPauseMenu();
        }
        if (_setupOpen) {
            ui::SetInputEnabled(!_help && !_confirmNew);
            _DrawSetup();
        }
        if (_archivesOpen) {
            ui::SetInputEnabled(!_help);
            _DrawArchives();
        }
        if (_confirmNew) {
            ui::SetInputEnabled(!_help);
            _DrawConfirm();
        }
        if (_settingsOpen) {
            ui::SetInputEnabled(!_help);
            _DrawSettings();
        }
        if (_help) {
            ui::SetInputEnabled(true);
            _DrawHelp();
        }
        ui::SetInputEnabled(true);
        _DrawToast();
    EndTextureMode();

    float impact = _fx.Impact() * (_settings.reduceFlashes ? 0.2f : 1.0f);
    _post.Present(_canvas.texture, _t, _settings.crt, impact);
}

// ─── Menu principal ──────────────────────────────────────────────────────────

void Game::_DrawMenu() {
    // Journal de démarrage façon terminal
    struct BootLine { const char* text; const char* status; Color color; };
    static const BootLine BOOT[] = {
        { "AUGATON NAV-OS",            VERSION_TAG, C_TEXT_DIM },
        { "Diagnostic du réacteur",    "OK",        C_GOOD },
        { "Matrice de boucliers",      "OK",        C_GOOD },
        { "Liaison propulseur FTL",    "OK",        C_GOOD },
        { "Données de mission",        "CHIFFRÉES", C_SHIELD },
        { "Flotte rebelle",            "EN APPROCHE", C_ENEMY },
    };
    for (int i = 0; i < 6; i++) {
        float appear = 0.18f * (float)i;
        if (_bootT < appear) break;
        float y = 22.0f + i * 20.0f;
        Text(TextFormat("> %s", BOOT[i].text), 40, y, 15, C_TEXT_DIM);
        if (_bootT > appear + 0.25f) Text(TextFormat("[%s]", BOOT[i].status), 320, y, 15, BOOT[i].color);
    }

    // Titre avec aberration chromatique et glitch périodique
    const float tx = 40.0f, ty = 170.0f;
    bool glitch = std::fmod(_t, 3.7f) < 0.12f;
    float jx = glitch ? rng::Float(-8.0f, 8.0f) : 0.0f;
    BeginBlendMode(BLEND_ADDITIVE);
        Label("AUGATON", tx - 4 + jx, ty, 100, ColorAlpha(C_ENEMY, 0.8f));
        Label("AUGATON", tx + 4 - jx, ty, 100, ColorAlpha(C_SHIELD, 0.8f));
    EndBlendMode();
    Label("AUGATON", tx, ty, 100, C_TEXT);

    Hazard({ tx, ty + 112, 600, 12 }, C_ACCENT, _t * 30.0f);
    Label("SURVIVEZ. SEMEZ LA FLOTTE. ABATTEZ L'AMIRAL.", tx, ty + 136, 20, C_ACCENT);

    // Menu
    const float bx = tx, bw = 380, bh = 42;
    const float step = 50;
    float by = 350;
    if (_hasSave) {
        if (Button({ bx, by, bw, bh }, "CONTINUER", true, C_GOOD, 20)) _Defer([this] { _ContinueRun(); });
        by += step;
    }
    if (Button({ bx, by, bw, bh }, "NOUVELLE PARTIE", true, _hasSave ? C_ACCENT : C_GOOD, 20))
        _Defer([this] { _setupOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "ARCHIVES", true, C_SCRAP, 20))        _Defer([this] { _archivesOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "PARAMÈTRES", true, C_ACCENT, 20))     _Defer([this] { _settingsOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "COMMENT JOUER", true, C_ACCENT, 20))  _Defer([this] { _help = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "QUITTER", true, C_ENEMY, 20))         _Defer([this] { _quit = true; });

    if (!_saveError.empty())
        Text(("Sauvegarde ignorée : " + _saveError).c_str(), bx, by + 60, 14, C_HULL_LOW);

    // Pied de page
    DrawRectangleRec({ 0, VIRTUAL_H - 30.0f, (float)VIRTUAL_W, 30 }, C_ACCENT);
    Label(TextFormat("%s  //  FLECHES + ENTREE  //  F1 AIDE  //  F11 PLEIN ECRAN  //  F12 CAPTURE", VERSION_TAG),
          16, VIRTUAL_H - 20.0f, 10, C_INK);
    bool blink = std::fmod(_t, 1.0f) < 0.5f;
    LabelRight(blink ? "PRET_" : "PRET", VIRTUAL_W - 16.0f, VIRTUAL_H - 20.0f, 10, C_INK);
}

// ─── Balise ──────────────────────────────────────────────────────────────────

void Game::_DrawBeaconUI() {
    const Beacon& b = _map.Current();
    float x = 676.0f;

    Label(TextFormat("BALISE // %s", SectorMap::KindName(b.kind)), x, 18, 10, C_LABEL);
    int toExit = _map.JumpsToExit(_map.CurrentIdx());
    if (toExit > 0)
        Label(TextFormat("SORTIE A %d SAUT%s", toExit, toExit > 1 ? "S" : ""), x, 34, 10, C_TEXT_DIM);

    Rectangle r = { x, 54, 260, 40 };
    if (Button(r, "VAISSEAU [U]", true, C_SHIELD, 20))
        _Defer([this] { _shipMenu.Open(); });
    r.y += 50;
    if (b.kind == BeaconKind::STORE) {
        if (Button(r, "MAGASIN", true, C_GOOD, 20))
            _Defer([this] {
                _store.Generate(_map.Sector(), _map.Sector() * 100 + _map.CurrentIdx());
                _store.Open();
                _state = GameState::STORE;
            });
        r.y += 50;
    }
    if (b.kind == BeaconKind::EXIT) {
        if (Button(r, "SECTEUR SUIVANT", true, C_WARNING, 20))
            _Defer([this] { _BeginSectorJump(); });
        r.y += 50;
    }
    if (b.kind == BeaconKind::BOSS) {
        if (Button(r, "ATTAQUER L'AMIRAL", true, C_ENEMY, 20))
            _Defer([this] { _StartCombat(EnemyKind::BOSS); });
    }
}

// ─── Saut ────────────────────────────────────────────────────────────────────

void Game::_DrawJump() {
    float k = std::clamp(_jumpT / JUMP_DURATION, 0.0f, 1.0f);
    float a = std::pow(std::sin(k * PI), 3.0f) * 0.9f;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(BLACK, a));

    Rectangle box = { VIRTUAL_W / 2.0f - 220, 440, 440, 70 };
    DrawRectangleRec(box, ColorAlpha(C_INK, 0.85f));
    DrawRectangleLinesEx(box, 2.0f, C_SHIELD);
    LabelCentered("SAUT FTL", box.x + box.width / 2, box.y + 10, 20, C_SHIELD);
    const int SEG = 20;
    float segW = (box.width - 40) / SEG;
    for (int i = 0; i < SEG; i++) {
        Rectangle s = { box.x + 20 + i * segW + 1, box.y + 40, segW - 2, 14 };
        if ((float)i < k * SEG) DrawRectangleRec(s, C_SHIELD);
        else                    DrawRectangleLinesEx(s, 1.0f, ColorAlpha(C_SHIELD, 0.4f));
    }
}

// ─── Événement ───────────────────────────────────────────────────────────────

void Game::_DrawEvent() {
    const float W  = 720.0f;
    const float x  = (VIRTUAL_W - W) / 2.0f;
    const float tw = W - 60.0f;
    const float bodySize = 18.0f;

    int  len      = _EventTextLength();
    bool revealed = _typed >= (float)len;
    int  shown    = revealed ? -1 : (int)_typed;

    // Hauteur calculée sur le texte complet : le panneau ne bouge pas pendant la frappe
    float h = 86.0f;
    if (!_eventBody.empty()) h += TextWrapped(_eventBody.c_str(), { 0, 0, tw, 0 }, bodySize, WHITE, false, false) + 14;
    if (_eventResolved) {
        if (!_resultText.empty()) h += TextWrapped(_resultText.c_str(), { 0, 0, tw, 0 }, bodySize, WHITE, false, false) + 12;
        h += (float)_resultLines.size() * 26.0f + 70.0f;
    } else if (_event) {
        h += (float)_event->choices.size() * 52.0f + 8.0f;
    }

    float ease = 1.0f - std::pow(1.0f - std::clamp(_eventAnim, 0.0f, 1.0f), 3.0f);
    float y = std::max(40.0f, 330.0f - h / 2.0f) + (1.0f - ease) * 24.0f;
    Dim(0.4f * ease);
    Panel({ x, y, W, h }, C_ACCENT, 0.3f, C_PANEL_BG, "TRANSMISSION ENTRANTE");

    Label(_eventTitle.c_str(), x + 30, y + 26, 30, C_TEXT);
    DrawRectangleRec({ x + 30, y + 62, tw, 2 }, C_BORDER);
    float cy = y + 76;

    if (!_eventBody.empty()) {
        Color bodyCol = _eventResolved ? C_TEXT_DIM : C_LABEL;
        cy += TextWrapped(_eventBody.c_str(), { x + 30, cy, tw, 0 }, bodySize, bodyCol,
                          false, true, _eventResolved ? -1 : shown) + 14;
    }

    bool cursor = std::fmod(_t, 0.6f) < 0.3f;

    if (!_eventResolved && _event) {
        if (!revealed) {
            if (cursor) Label("_", x + W - 44, y + h - 34, 20, C_ACCENT);
            return;
        }
        for (int i = 0; i < (int)_event->choices.size(); i++) {
            const Choice& c = _event->choices[i];
            bool ok = ChoiceAvailable(c, _player);
            std::string label = std::to_string(i + 1) + ". " + c.label;
            std::string req = ChoiceRequirement(c);
            if (!req.empty()) label += "  " + req;

            Color accent = c.success.combat ? C_ENEMY : (c.chance < 1.0f ? C_WARNING : C_ACCENT);
            if (ButtonText({ x + 30, cy, tw, 42 }, label.c_str(), ok, accent, 17))
                _Defer([this, i] { _ChooseOption(i); });
            cy += 52;
        }
    } else {
        if (!_resultText.empty())
            cy += TextWrapped(_resultText.c_str(), { x + 30, cy, tw, 0 }, bodySize, C_TEXT, false, true, shown) + 12;
        if (!revealed) {
            if (cursor) Label("_", x + W - 44, y + h - 34, 20, C_ACCENT);
            return;
        }

        for (const auto& line : _resultLines) {
            if (line.empty()) continue;
            Color c = line[0] == '+' ? C_GOOD : line[0] == '-' ? C_HULL_LOW : C_SCRAP;
            DrawRectangleRec({ x + 30, cy + 2, 8, 16 }, c);
            Text(line.c_str(), x + 48, cy, 18, c);
            cy += 26;
        }
        cy += 14;
        if (_OutcomeIsPlain()) {
            if (Button({ x + W / 2 - 250, cy, 240, 42 }, "CARTE [J]", true, C_SHIELD, 20))
                _Defer([this] { _ContinueToMap(); });
            if (Button({ x + W / 2 + 10, cy, 240, 42 }, "RESTER [ESPACE]", true, C_ACCENT, 20))
                _Defer([this] { _ContinueAfterEvent(); });
        } else {
            const char* label = _pendingOutcome.combat ? "COMBAT ! [ESPACE]"
                              : _pendingOutcome.store  ? "COMMERCER [ESPACE]"
                                                       : "SAUTER [ESPACE]";
            if (Button({ x + W / 2 - 130, cy, 260, 42 }, label, true, _pendingOutcome.combat ? C_ENEMY : C_ACCENT, 20))
                _Defer([this] { _ContinueAfterEvent(); });
        }
    }
}

// ─── Fin de partie ───────────────────────────────────────────────────────────

void Game::_DrawEndScreen(bool victory) {
    Dim(victory ? 0.4f : 0.6f);

    Rectangle r = { 370, 100, 540, 520 };
    Color accent = victory ? C_GOOD : C_HULL_LOW;
    Panel(r, accent, 0.4f, C_PANEL_BG, victory ? "MISSION ACCOMPLIE" : "SIGNAL PERDU");

    Hazard({ r.x + 2, r.y + 2, r.width - 4, 16 }, ColorAlpha(accent, 0.5f), _t * 20.0f);
    LabelCentered(victory ? "VICTOIRE" : "VAISSEAU DETRUIT", r.x + r.width / 2, r.y + 36, 40, accent);

    float y = r.y + 86;
    LabelCentered(TextFormat("%s  //  %s", GetPlayerShipInfo(_player.shipType).name, GetDifficulty(_difficulty).name),
                  r.x + r.width / 2, y, 10, C_TEXT_DIM);
    y += 20;
    y += TextWrapped(_endReason.c_str(), { r.x + 36, y, r.width - 72, 0 }, 16, C_LABEL, true) + 14;

    struct Line { const char* label; int value; };
    Line lines[] = {
        { "SECTEUR ATTEINT",     _map.Sector() + 1 },
        { "SAUTS",               _stats.jumps },
        { "ENNEMIS DETRUITS",    _stats.kills },
        { "EVENEMENTS",          _stats.events },
        { "FERRAILLE RECOLTEE",  _stats.scrap },
    };
    for (const auto& l : lines) {
        Label(l.label, r.x + 60, y, 20, C_TEXT_DIM);
        LabelRight(TextFormat("%d", l.value), r.x + r.width - 60, y, 20, C_TEXT);
        y += 30;
    }

    for (int id : _newUnlocks) {
        bool blink = std::fmod(_t, 0.8f) < 0.5f;
        const char* msg = TextFormat("NOUVEAU VAISSEAU : %s", GetPlayerShipInfo(id).name);
        float w = MeasureLabel(msg, 20) + 24;
        DrawRectangleRec({ r.x + (r.width - w) / 2, y + 4, w, 28 }, blink ? C_SCRAP : ColorAlpha(C_SCRAP, 0.3f));
        LabelCentered(msg, r.x + r.width / 2, y + 8, 20, blink ? C_INK : C_SCRAP);
        y += 34;
    }

    float by = r.y + r.height - 70;
    if (Button({ r.x + 40, by, 210, 46 }, "REJOUER", true, C_GOOD, 20))
        _Defer([this] { _NewGame(); });
    if (Button({ r.x + r.width - 250, by, 210, 46 }, "MENU", true, C_ACCENT, 20))
        _Defer([this] { _ReturnToMenu(); });
}

// ─── Surcouches ──────────────────────────────────────────────────────────────

void Game::_DrawPauseMenu() {
    Dim(0.65f);
    Rectangle r = { 470, 170, 340, 380 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "SYSTEME EN ATTENTE");
    LabelCentered("PAUSE", r.x + r.width / 2, r.y + 28, 40, C_ACCENT);

    float bx = r.x + 30, bw = r.width - 60, by = r.y + 96;
    if (Button({ bx, by, bw, 44 }, "REPRENDRE", true, C_GOOD, 20))          _Defer([this] { _pause = false; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "PARAMÈTRES", true, C_ACCENT, 20))       _Defer([this] { _settingsOpen = true; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "COMMANDES", true, C_ACCENT, 20))        _Defer([this] { _help = true; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "MENU PRINCIPAL", true, C_WARNING, 20))  _Defer([this] { _ReturnToMenu(); });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "QUITTER", true, C_ENEMY, 20))           _Defer([this] { _quit = true; });

    Text("Progression sauvegardée à chaque balise.", r.x + 30, r.y + r.height - 26, 13, C_TEXT_DIM);
}

void Game::_DrawSettings() {
    Dim(0.7f);
    Rectangle r = { 250, 40, 780, 640 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "CONFIGURATION");
    Label("PARAMETRES", r.x + 30, r.y + 26, 30, C_TEXT);

    auto pct   = [](float v) { return TextFormat("%d%%", (int)std::lround(v * 100.0f)); };
    auto yesno = [](bool v) { return v ? "OUI" : "NON"; };
    auto step  = [](float v, int d) { return std::clamp(std::round((v + 0.1f * d) * 10.0f) / 10.0f, 0.0f, 1.0f); };

    bool changed = false;
    float x = r.x + 30, w = r.width - 60, y = r.y + 76;
    const float H = 32, GAP = 3;
    int d;

    if ((d = Stepper({ x, y, w, H }, "VOLUME GENERAL", pct(_settings.masterVolume)))) { _settings.masterVolume = step(_settings.masterVolume, d); changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "MUSIQUE", pct(_settings.musicVolume))))          { _settings.musicVolume = step(_settings.musicVolume, d); changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "EFFETS SONORES", pct(_settings.sfxVolume))))     { _settings.sfxVolume = step(_settings.sfxVolume, d); changed = true; }
    y += H + GAP + 8;

    if ((d = Stepper({ x, y, w, H }, "PLEIN ECRAN", yesno(_settings.fullscreen))))     { _settings.fullscreen = !_settings.fullscreen; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "SYNCHRO VERTICALE", yesno(_settings.vsync))))    { _settings.vsync = !_settings.vsync; changed = true; }
    y += H + GAP;
    {
        static const int CAPS[] = { 30, 60, 120, 144, 0 };
        int idx = 1;
        for (int i = 0; i < 5; i++) if (CAPS[i] == _settings.fpsCap) idx = i;
        const char* label = _settings.fpsCap != 0 ? TextFormat("%d", _settings.fpsCap)
                          : _settings.vsync       ? "ECRAN" : "ILLIMITE";
        if ((d = Stepper({ x, y, w, H }, "LIMITE D'IMAGES/S", label))) {
            _settings.fpsCap = CAPS[(idx + d + 5) % 5];
            changed = true;
        }
    }
    y += H + GAP + 8;

    static const char* CRT[] = { "AUCUN", "LEGER", "FORT" };
    if ((d = Stepper({ x, y, w, H }, "EFFET CATHODIQUE", CRT[std::clamp(_settings.crt, 0, 2)]))) {
        _settings.crt = (_settings.crt + d + 3) % 3; changed = true;
    }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "TREMBLEMENTS", pct(_settings.shake)))) {
        _settings.shake = std::clamp(_settings.shake + 0.5f * d, 0.0f, 1.0f); changed = true;
    }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "REDUIRE LES FLASHS", yesno(_settings.reduceFlashes)))) { _settings.reduceFlashes = !_settings.reduceFlashes; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "PAUSE HORS FOCUS", yesno(_settings.pauseOnFocus))))    { _settings.pauseOnFocus = !_settings.pauseOnFocus; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "PAUSE AUTO EN COMBAT", yesno(_settings.autoPause))))   { _settings.autoPause = !_settings.autoPause; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "TUTORIEL", yesno(_settings.tutorial)))) {
        _settings.tutorial = !_settings.tutorial;
        if (_settings.tutorial) _tutStep = 0;
        changed = true;
    }
    y += H + GAP;
    static const char* SPEED[] = { "INSTANTANE", "NORMAL", "RAPIDE" };
    if ((d = Stepper({ x, y, w, H }, "VITESSE DU TEXTE", SPEED[std::clamp(_settings.textSpeed, 0, 2)]))) {
        _settings.textSpeed = (_settings.textSpeed + d + 3) % 3; changed = true;
    }

    if (changed) _Defer([this] { _ApplySettings(); });

    Text("Flèches haut/bas : choisir   gauche/droite : modifier", r.x + 30, r.y + r.height - 44, 14, C_TEXT_DIM);
    if (Button({ r.x + r.width - 210, r.y + r.height - 60, 180, 42 }, "RETOUR", true, C_ACCENT, 20))
        _Defer([this] { _settingsOpen = false; _settings.Save(); });
}

void Game::_LaunchFromSetup() {
    if (_hasSave) _confirmNew = true;
    else          { _setupOpen = false; _NewGame(); }
}

void Game::_DrawSetup() {
    Dim(0.75f);
    Rectangle p = { 120, 40, 1040, 640 };
    Panel(p, C_GOOD, 0.3f, C_PANEL_BG, "NOUVELLE PARTIE");
    Label("CHOISISSEZ VOTRE VAISSEAU", p.x + 24, p.y + 22, 20, C_TEXT);

    static Ship previews[PLAYER_SHIP_COUNT];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < PLAYER_SHIP_COUNT; i++) previews[i] = MakePlayerShip(i);
        built = true;
    }

    const float cw = 326, ch = 400, gap = 11;
    for (int i = 0; i < PLAYER_SHIP_COUNT; i++) {
        Rectangle c = { p.x + 20 + i * (cw + gap), p.y + 56, cw, ch };
        const Ship& ship = previews[i];
        const PlayerShipInfo& info = GetPlayerShipInfo(i);
        bool unlocked = _profile.unlocked[i];
        bool selected = _selectedShip == i;
        Color accent  = !unlocked ? C_BORDER : selected ? C_GOOD : C_ACCENT;

        if (unlocked && Hover(c) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) _Defer([this, i] { _selectedShip = i; });

        DrawRectangleRec(c, ColorAlpha(selected ? Color{ 10, 30, 16, 255 } : C_INK, unlocked ? 0.7f : 0.4f));
        DrawRectangleLinesEx(c, selected ? 2.0f : 1.0f, accent);
        if (selected) DrawRectangleRec({ c.x, c.y, c.width, 4 }, C_GOOD);

        Label(info.name, c.x + 16, c.y + 16, 30, unlocked ? C_TEXT : C_TEXT_DIM);
        Text(info.role, c.x + 18, c.y + 52, 14, unlocked ? accent : C_TEXT_DIM);

        // Aperçu du vaisseau
        ShipView view;
        view.pos       = { c.x + c.width / 2, c.y + 140 };
        view.rotation  = 90.0f + std::sin(_t * 0.8f + i) * 3.0f;
        view.scaleMult = 110.0f / _renderer.Size(ship);
        view.alpha     = unlocked ? 1.0f : 0.25f;
        _renderer.DrawShip(ship, view, _t);

        if (!unlocked) {
            LabelCentered("VERROUILLE", c.x + c.width / 2, c.y + 130, 20, C_WARNING);
            TextWrapped(info.unlockHint, { c.x + 20, c.y + 220, c.width - 40, 0 }, 15, C_LABEL, true);
            continue;
        }

        // Caractéristiques
        float y = c.y + 206;
        Text(TextFormat("Coque %d   ·   Réacteur %d   ·   Missiles %d", ship.maxHull, ship.reactor, ship.missiles),
             c.x + 18, y, 13, C_TEXT);
        y += 20;
        // Lignes composées dans des tampons fixes (écran redessiné à chaque image)
        char mods[128] = "", arms[160] = "";
        size_t mu = 0, au = 0;
        for (int sidx = 0; sidx < SYS_COUNT && mu < sizeof mods; sidx++) {
            if (!ship.sys[sidx].Installed()) continue;
            int n = std::snprintf(mods + mu, sizeof mods - mu, "%s%d ", SystemShortName(sidx), ship.sys[sidx].level);
            if (n < 0) break;
            mu = std::min(sizeof mods - 1, mu + (size_t)n);
        }
        Text(mods, c.x + 18, y, 13, C_LABEL);
        y += 20;
        for (const auto& w : ship.weapons) {
            if (w.Empty() || au >= sizeof arms) continue;
            int n = std::snprintf(arms + au, sizeof arms - au, "%s%s", au == 0 ? "" : " + ", GetWeaponDef(w.id).name);
            if (n < 0) break;
            au = std::min(sizeof arms - 1, au + (size_t)n);
        }
        Text(arms, c.x + 18, y, 13, C_ENEMY);
        y += 20;
        for (int a : ship.augments)
            if (a >= 0) { Text(GetAugDef(a).name, c.x + 18, y, 13, C_SCRAP); y += 20; }
        TextWrapped(info.description, { c.x + 18, y + 6, c.width - 36, 0 }, 13, C_TEXT_DIM);

        if (Button({ c.x + 20, c.y + ch - 50, c.width - 40, 36 }, selected ? "SELECTIONNE" : "CHOISIR", !selected, C_GOOD, 20))
            _Defer([this, i] { _selectedShip = i; });
    }

    // Difficulté
    float y = p.y + 56 + ch + 20;
    int d = Stepper({ p.x + 20, y, 520, 40 }, "DIFFICULTE", GetDifficulty(_difficulty).name, C_ACCENT);
    if (d != 0) _difficulty = (_difficulty + d + DIFFICULTY_COUNT) % DIFFICULTY_COUNT;
    static const char* DIFF_DESC[DIFFICULTY_COUNT] = {
        "Ennemis moins résistants et plus lents, +25 % de ferraille, flotte ralentie.",
        "L'expérience prévue.",
        "Ennemis blindés et agressifs, moins de ferraille, flotte plus rapide.",
    };
    Text(DIFF_DESC[_difficulty], p.x + 24, y + 50, 14, C_TEXT_DIM);

    float by = p.y + p.height - 62;
    if (Button({ p.x + p.width - 440, by, 200, 44 }, "RETOUR", true, C_ACCENT, 20))
        _Defer([this] { _setupOpen = false; });
    if (Button({ p.x + p.width - 220, by, 200, 44 }, "LANCER", _profile.unlocked[_selectedShip], C_GOOD, 20))
        _Defer([this] { _LaunchFromSetup(); });
}

void Game::_DrawArchives() {
    Dim(0.75f);
    Rectangle p = { 170, 40, 940, 640 };
    Panel(p, C_SCRAP, 0.3f, C_PANEL_BG, "ARCHIVES");
    Label("ARCHIVES DE VOL", p.x + 24, p.y + 22, 30, C_TEXT);

    struct Stat { const char* label; int value; };
    Stat stats[] = {
        { "PARTIES",           _profile.runs },
        { "VICTOIRES",         _profile.wins },
        { "MEILLEUR SECTEUR",  _profile.bestSector },
        { "ENNEMIS DETRUITS",  _profile.totalKills },
        { "FERRAILLE TOTALE",  _profile.totalScrap },
    };
    float x = p.x + 24, y = p.y + 76;
    for (const auto& st : stats) {
        Rectangle r = { x, y, 170, 64 };
        DrawRectangleRec(r, ColorAlpha(C_INK, 0.5f));
        DrawRectangleRec({ r.x, r.y, 3, r.height }, C_SCRAP);
        Label(st.label, r.x + 12, r.y + 10, 10, C_TEXT_DIM);
        Label(TextFormat("%d", st.value), r.x + 12, r.y + 28, 30, C_TEXT);
        x += 180;
    }

    // Vaisseaux
    y += 90;
    Label("VAISSEAUX", p.x + 24, y, 10, C_ACCENT);
    y += 18;
    for (int i = 0; i < PLAYER_SHIP_COUNT; i++) {
        const PlayerShipInfo& info = GetPlayerShipInfo(i);
        bool ok = _profile.unlocked[i];
        DrawRectangleRec({ p.x + 24, y, p.width - 48, 30 }, ColorAlpha(C_INK, 0.4f));
        Label(info.name, p.x + 36, y + 8, 20, ok ? C_TEXT : C_TEXT_DIM);
        Text(info.role, p.x + 200, y + 7, 14, C_LABEL);
        TextRight(ok ? "Débloqué" : info.unlockHint, p.x + p.width - 36, y + 7, 14, ok ? C_GOOD : C_WARNING);
        y += 36;
    }

    // Historique
    y += 16;
    Label("DERNIERES PARTIES", p.x + 24, y, 10, C_ACCENT);
    y += 20;
    const float cols[] = { 36, 200, 360, 480, 620, 740 };
    const char* heads[] = { "VAISSEAU", "DIFFICULTE", "SECTEUR", "RESULTAT", "ENNEMIS", "FERRAILLE" };
    for (int k = 0; k < 6; k++) Label(heads[k], p.x + cols[k], y, 10, C_TEXT_DIM);
    y += 18;
    if (_profile.history.empty())
        Text("Aucune partie terminée pour l'instant.", p.x + 36, y + 4, 14, C_TEXT_DIM);
    for (const auto& h : _profile.history) {
        Color c = h.won ? C_GOOD : C_LABEL;
        Text(GetPlayerShipInfo(h.ship).name,            p.x + cols[0], y, 14, c);
        Text(GetDifficulty(h.difficulty).name,          p.x + cols[1], y, 14, c);
        Text(TextFormat("%d / %d", h.sector, SECTOR_COUNT), p.x + cols[2], y, 14, c);
        Text(h.won ? "VICTOIRE" : "DÉTRUIT",            p.x + cols[3], y, 14, h.won ? C_GOOD : C_HULL_LOW);
        Text(TextFormat("%d", h.kills),                 p.x + cols[4], y, 14, c);
        Text(TextFormat("%d", h.scrap),                 p.x + cols[5], y, 14, c);
        y += 22;
    }

    if (Button({ p.x + p.width - 220, p.y + p.height - 62, 196, 44 }, "FERMER", true, C_SCRAP, 20))
        _Defer([this] { _archivesOpen = false; });
}

// ─── Tutoriel ────────────────────────────────────────────────────────────────

namespace {
struct TutorialStep {
    const char* title;
    const char* text;
    Vector2     pointer;   // élément désigné (coordonnées virtuelles)
};
const TutorialStep TUTORIAL[] = {
    { "ENERGIE",
      "Vos armes (en bas) se chargent quand elles sont alimentées. Les colonnes en bas à gauche répartissent "
      "l'énergie du réacteur : clic gauche pour ajouter, clic droit pour retirer.",
      { 150, 600 } },
    { "VISER",
      "Cliquez sur une salle du vaisseau ennemi pour y envoyer toutes vos armes. [T] choisit les cibles "
      "automatiquement : lasers sur les boucliers, missiles sur les armes.",
      { 940, 350 } },
    { "PAUSE ET VITESSE",
      "[ESPACE] fige le combat pour donner vos ordres calmement. [TAB] accélère le temps quand tout est sous contrôle.",
      { 760, 30 } },
    { "DEFENSE",
      "Les couches de bouclier bloquent les lasers, pas les missiles. Les salles encadrées en rouge sur votre "
      "vaisseau sont visées par l'ennemi : anticipez !",
      { 330, 360 } },
};
constexpr int TUTORIAL_STEPS = 4;
}

void Game::_UpdateTutorial(float dt) {
    if (!_settings.tutorial || _tutStep >= TUTORIAL_STEPS || _state != GameState::COMBAT) return;
    _tutTimer += dt;
    if (_combat.Paused()) _tutPausedOnce = true;

    bool done = false;
    switch (_tutStep) {
    case 0: done = _tutTimer > 8.0f; break;
    case 1:
        for (const auto& w : _player.weapons) if (!w.Empty() && w.target >= 0) done = true;
        break;
    case 2: done = _tutPausedOnce && !_combat.Paused(); break;
    case 3: done = _tutTimer > 9.0f; break;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) done = true;

    if (done) {
        _tutStep++;
        _tutTimer = 0.0f;
        _audio.PlaySfx("sx_ready", 1.3f, 0.4f);
        if (_tutStep >= TUTORIAL_STEPS) {
            _settings.tutorial = false;
            _settings.Save();
            _toast  = "Tutoriel terminé. [J] ouvre la carte, [U] le menu VAISSEAU. Réactivable dans les paramètres.";
            _toastT = 5.0f;
        }
    }
}

void Game::_DrawTutorial() {
    if (!_settings.tutorial || _tutStep >= TUTORIAL_STEPS || _combat.Dying()) return;
    const TutorialStep& st = TUTORIAL[_tutStep];

    const float W = 620.0f;
    float h = TextWrapped(st.text, { 0, 0, W - 40, 0 }, 15, WHITE, false, false) + 70;
    Rectangle r = { (VIRTUAL_W - W) / 2.0f, 440, W, h };

    // Flèche vers l'élément concerné
    float pulse = 0.5f + 0.5f * std::sin(_t * 6.0f);
    Vector2 from = { r.x + r.width / 2, st.pointer.y < r.y ? r.y : r.y + r.height };
    DrawLineEx(from, st.pointer, 2.0f, ColorAlpha(C_GOOD, 0.4f + 0.4f * pulse));
    DrawCircleLinesV(st.pointer, 16.0f + 6.0f * pulse, C_GOOD);

    DrawRectangleRec(r, ColorAlpha(C_INK, 0.9f));
    DrawRectangleLinesEx(r, 2.0f, C_GOOD);
    DrawRectangleRec({ r.x, r.y, r.width, 22 }, C_GOOD);
    Label(TextFormat("TUTORIEL %d/%d - %s", _tutStep + 1, TUTORIAL_STEPS, st.title), r.x + 12, r.y + 6, 10, C_INK);
    TextWrapped(st.text, { r.x + 20, r.y + 32, W - 40, 0 }, 15, C_TEXT);
    const char* hint = _tutStep == 1 ? "Assignez une cible pour continuer"
                     : _tutStep == 2 ? "Appuyez sur ESPACE deux fois pour continuer"
                                     : "Continue automatiquement";
    Text(hint, r.x + 20, r.y + h - 26, 13, C_GOOD);
    TextRight("[ENTREE] étape suivante", r.x + W - 20, r.y + h - 26, 13, C_TEXT_DIM);
}

void Game::_DrawConfirm() {
    Dim(0.7f);
    Rectangle r = { 390, 250, 500, 220 };
    Panel(r, C_WARNING, 0.4f, C_PANEL_BG, "CONFIRMATION");
    LabelCentered("NOUVELLE PARTIE ?", r.x + r.width / 2, r.y + 30, 30, C_WARNING);
    TextWrapped("La partie en cours sera définitivement effacée.", { r.x + 40, r.y + 84, r.width - 80, 0 },
                17, C_LABEL, true);
    if (Button({ r.x + 40, r.y + 150, 200, 44 }, "ANNULER", true, C_ACCENT, 20))
        _Defer([this] { _confirmNew = false; });
    if (Button({ r.x + r.width - 240, r.y + 150, 200, 44 }, "EFFACER", true, C_ENEMY, 20))
        _Defer([this] { _confirmNew = false; _setupOpen = false; _NewGame(); });
}

void Game::_DrawHelp() {
    Dim(0.75f);
    Rectangle r = { 180, 20, 920, 680 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "MANUEL DE BORD");
    Label("COMMENT JOUER", r.x + 30, r.y + 22, 30, C_TEXT);

    struct Entry { const char* key; const char* desc; };
    const Entry controls[] = {
        { "CLIC G / MOLETTE+", "Ajouter de l'énergie à un système" },
        { "CLIC D / MOLETTE-", "Retirer de l'énergie" },
        { "CLIC G SUR ARME",   "Allumer l'arme, puis la sélectionner" },
        { "CLIC SALLE ENNEMIE","Assigner la cible de l'arme sélectionnée" },
        { "CLIC D SUR ARME",   "Éteindre l'arme" },
        { "GLISSER UNE ARME",  "Changer d'emplacement / viser une salle ennemie" },
        { "MAJ + CLIC G / D",  "Système au maximum / coupé, toutes les armes" },
        { "1 - 4",             "Sélectionner / allumer une arme" },
        { "ESPACE",            "Pause tactique en combat / continuer" },
        { "TAB",               "Vitesse du combat x1 / x2 / x4" },
        { "T / CLIC SALLE",    "Ciblage auto / toutes les armes sur la salle" },
        { "C / H",             "Invisibilité / piratage (modules achetés)" },
        { "U",                 "Menu VAISSEAU : modules et augmentations" },
        { "J",                 "Carte du secteur, saut FTL d'urgence" },
        { "1 - 9 (CARTE)",     "Sauter vers la balise numérotée" },
        { "MAJ + CLIC",        "Acheter par 5 au magasin" },
        { "FLECHES + ENTREE",  "Naviguer dans les menus" },
        { "ECHAP / F1",        "Annuler, menu pause / aide" },
        { "F11 / F12",         "Plein écran / capture d'écran" },
    };
    float y = r.y + 74;
    for (const auto& c : controls) {
        DrawRectangleRec({ r.x + 30, y - 2, 250, 21 }, ColorAlpha(C_ACCENT, 0.15f));
        Label(c.key, r.x + 38, y + 3, 10, C_ACCENT);
        Text(c.desc, r.x + 300, y, 16, C_LABEL);
        y += 23;
    }

    y += 10;
    Label("CONSEILS", r.x + 30, y, 20, C_TEXT);
    y += 28;
    TextWrapped("Les boucliers bloquent les lasers : percez-les avec des missiles ou des rafales. "
                "Toucher les ARMES ennemies les empêche de tirer, toucher les MOTEURS réduit leur esquive. "
                "Chaque saut fait avancer la flotte rebelle. Les systèmes sont réparés après le combat, "
                "pas la coque : utilisez les magasins.",
                { r.x + 30, y, r.width - 290, 0 }, 15, C_LABEL);   // laisse la place au bouton

    if (Button({ r.x + r.width - 210, r.y + r.height - 60, 180, 42 }, "FERMER", true, C_ACCENT, 20))
        _Defer([this] { _help = false; });
}

void Game::_DrawToast() {
    if (_toastT <= 0.0f || _toast.empty()) return;
    float a = std::min(1.0f, _toastT);
    float w = Measure(_toast.c_str(), 15) + 30;
    Rectangle r = { (VIRTUAL_W - w) / 2.0f, 150, w, 30 };
    DrawRectangleRec(r, ColorAlpha(C_INK, 0.9f * a));
    DrawRectangleLinesEx(r, 2.0f, ColorAlpha(C_ACCENT, a));
    Text(_toast.c_str(), r.x + 15, r.y + 7, 15, ColorAlpha(C_TEXT, a));
}
