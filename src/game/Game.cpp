#include "Game.hpp"
#include "../audio/Synth.hpp"
#include "../core/Config.hpp"
#include "../core/Paths.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include "../ship/ShipFactory.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

using namespace ui;

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

    ui::Load();
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
    ui::Unload();
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
    if (!_hasSave) { _saveError = err; return; }
    _saveInfo = { probe.ship.hull, probe.ship.maxHull, probe.ship.scrap, probe.ship.fuel, probe.map.sector };
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
    // Fond procédural en basse résolution (tramé), calculé seulement au changement de balise
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
        // Le fond, opaque, recouvre tout le canevas : pas d'effacement préalable
        _post.DrawBackground(_bg.GetFtlLerp(), _bg.Impact());

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
