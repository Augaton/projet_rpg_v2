#include "Combat.hpp"
#include "../audio/AudioManager.hpp"
#include "../core/Config.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include "../effect/Effects.hpp"
#include "../hud/HUD.hpp"
#include "../projectile/ProjectileManager.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

static constexpr float BURST_DELAY = 0.14f;
static constexpr float SIM_STEP    = 1.0f / 60.0f;

void Combat::Init(AudioManager* audio, ProjectileManager* proj, Effects* fx,
                  const ShipRenderer* renderer, HUD* hud) {
    _audio    = audio;
    _proj     = proj;
    _fx       = fx;
    _renderer = renderer;
    _hud      = hud;
}

void Combat::Begin(Ship* player, ShipView* playerView, const Ship& enemy, EnemyKind kind, BeaconHazard hazard) {
    _hazard      = hazard;
    _hazardTimer = hazard == BeaconHazard::SOLAR_FLARE ? 14.0f : hazard == BeaconHazard::ION_STORM ? 9.0f : 4.0f;
    _rocks.clear();
    if (hazard == BeaconHazard::ASTEROIDS) {
        for (int i = 0; i < 26; i++)
            _rocks.push_back({ { rng::Float(0.0f, (float)VIRTUAL_W), rng::Float(120.0f, 560.0f) },
                               rng::Float(4.0f, 18.0f), rng::Float(0.0f, 360.0f),
                               rng::Float(-40.0f, 40.0f), rng::Float(10.0f, 45.0f) });
    }

    _player     = player;
    _playerView = playerView;
    _enemy      = enemy;
    _kind       = kind;

    _enemyHome          = { 940.0f, 350.0f };
    _enemyView          = ShipView{};
    _enemyView.pos      = { VIRTUAL_W + 220.0f, _enemyHome.y };
    _enemyView.rotation = 270.0f;
    _playerView->destroyT = -1.0f;

    _player->PrepareForCombat();
    _enemy.PrepareForCombat();
    for (auto& w : _player->weapons)
        if (!w.Empty() && w.preferred >= 0 && w.preferred < SYS_COUNT && _enemy.sys[w.preferred].Installed())
            w.target = w.preferred;
    _idleReady = 0.0f;
    _bossPhase   = 1;
    _phaseBanner = 0.0f;
    _hackSelecting = false;
    _beams.clear();
    _playerView->alpha = 1.0f;
    // L'ennemi démarre avec un peu d'avance pour ne pas tirer en rafale synchronisée
    for (auto& w : _enemy.weapons) w.charge = rng::Float(0.0f, 0.35f);

    _shots.clear();
    _proj->Clear();

    _active       = true;
    _paused       = false;
    _selected     = -1;
    _hoverRoom    = -1;
    _aiTimer      = 0.0f;
    _enemyDeathT  = -1.0f;
    _playerDeathT = -1.0f;

    _elapsed   = 0.0f;
    _hitStop   = 0.0f;
    _slowmo    = 0.0f;
    _timeScale = 1.0f;

    _flee       = FleeState::NONE;
    _fleeTimer  = 0.0f;
    _fleeCharge = 0.0f;
    _fleeEval   = 4.0f;
    _fleeTried  = false;

    _prevHull = _player->hull;
    _AutoPause("Pause auto : début du combat");
}

void Combat::_AutoPause(const char* reason) {
    if (!_autoPause || _paused || !_active || Dying()) return;
    _paused = true;
    _hud->Log(reason, ui::C_WARNING);
}

void Combat::End() {
    _active   = false;
    _paused   = false;
    _selected = -1;
    _shots.clear();
    _proj->Clear();
    if (_player) {
        for (auto& w : _player->weapons) {
            w.target = -1;
            w.pendingShots = 0;
        }
    }
    if (_playerView) { _playerView->kick = { 0, 0 }; _playerView->alpha = 1.0f; }
    _hackSelecting = false;
    _beams.clear();
}

bool Combat::CancelTargeting() {
    if (_hackSelecting) { _hackSelecting = false; return true; }
    if (_selected < 0) return false;
    _selected = -1;
    return true;
}

float Combat::FleeValue() const {
    switch (_flee) {
    case FleeState::INTENT:   return std::max(0.0f, _fleeTimer);
    case FleeState::CHARGING: return std::clamp(_fleeCharge, 0.0f, 1.0f);
    default:                  return 0.0f;
    }
}

Rectangle Combat::AutoTargetRect() { return { 976.0f, 652.0f, 120.0f, 30.0f }; }
Rectangle Combat::AbilityRect(int idx) { return { 16.0f + idx * 158.0f, 504.0f, 150.0f, 30.0f }; }

void Combat::_ToggleCloak() {
    Ship& p = *_player;
    if (!p.sys[SYS_CLOAK].Installed() || Dying()) return;
    if (p.ActivateCloak()) {
        _hud->Log(TextFormat("Invisibilité : %.0fs", p.CloakDuration()), ui::C_SHIELD);
        _fx->Shockwave(_playerView->pos, 60.0f, ui::C_SHIELD);
        _audio->PlaySfx("sx_cloak", 1.0f, 0.6f);
    } else if (p.Cloaked()) {
        _audio->PlaySfx("sx_error", 1.0f, 0.4f);
        _hud->Log("Déjà invisible", ui::C_TEXT_DIM);
    } else if (p.sys[SYS_CLOAK].power <= 0) {
        _audio->PlaySfx("sx_error", 1.0f, 0.4f);
        _hud->Log("Invisibilité : module non alimenté", ui::C_WARNING);
    } else {
        _audio->PlaySfx("sx_error", 1.0f, 0.4f);
        _hud->Log(TextFormat("Invisibilité en recharge (%.0fs)", p.cloakCooldown), ui::C_WARNING);
    }
}

void Combat::_ToggleHackSelect() {
    Ship& p = *_player;
    if (!p.sys[SYS_HACKING].Installed() || Dying()) return;
    if (_hackSelecting) { _hackSelecting = false; return; }
    if (!p.CanHack()) {
        _audio->PlaySfx("sx_error", 1.0f, 0.4f);
        _hud->Log(p.sys[SYS_HACKING].power <= 0 ? "Piratage : module non alimenté"
                                                 : TextFormat("Piratage en recharge (%.0fs)", p.hackCooldown), ui::C_WARNING);
        return;
    }
    _hackSelecting = true;
    _selected      = -1;
    _hud->Log("Piratage : cliquez sur une salle ennemie", ui::C_ION);
}

void Combat::_Hack(int sys) {
    Ship& p = *_player;
    if (!p.CanHack() || sys < 0 || !SystemIsRoom(sys) || !_enemy.sys[sys].Installed()) return;
    float duration = p.HackDuration();
    _enemy.sys[sys].hackTimer = duration;
    _enemy.RecomputePower();
    p.hackCooldown = duration + 14.0f;
    _hackSelecting = false;

    Vector2 room = _renderer->RoomPos(_enemy, _enemyView, sys);
    _beams.push_back({ _renderer->MuzzlePos(p, *_playerView), room, ui::C_ION, 0.5f });
    _fx->Spark(room, ui::C_ION, 24, 180.0f, 3.0f, 0.6f);
    _fx->Text(TextFormat("%s PIRATÉ", SystemShortName(sys)), { room.x, room.y - 40 }, ui::C_ION, 20, true);
    _hud->Log(TextFormat("%s piraté pour %.0fs", SystemName(sys), duration), ui::C_ION);
    _audio->PlaySfx("sx_hack", 1.0f, 0.6f);
}

// Défense anti-missile : tente d'abattre un missile au contact du bouclier
bool Combat::_TryIntercept(Ship& target, const ShipView& tv, Projectile& p) {
    if (target.sys[SYS_DEFENSE].power <= 0 || target.defenseCooldown > 0.0f) return false;
    target.defenseCooldown = 1.2f;
    if (!rng::Chance(target.DefenseChance())) return false;

    Vector2 turret = _renderer->RoomPos(target, tv, SYS_DEFENSE);
    _beams.push_back({ turret, p.pos, ui::C_WARNING, 0.25f });
    _fx->Explosion(p.pos, 0.5f);
    _fx->Text("INTERCEPTÉ", { p.pos.x, p.pos.y - 24 }, ui::C_WARNING, 20, true);
    _audio->PlaySfx("gun_2", 1.5f, 0.5f);
    return true;
}

void Combat::_AssignTarget(int slot, int sys) {
    WeaponSlot& w = _player->weapons[slot];
    w.target    = sys;
    w.preferred = sys;
}

// Cible conseillée (validée par simulation) :
//  - missiles : ils traversent les boucliers → désarmer l'ennemi (ARMES) ;
//  - lasers / ion : faire tomber les BOUCLIERS, puis viser les ARMES ;
//  - ennemi en fuite : les gros coups vont dans les MOTEURS.
int Combat::_BestTarget(int weaponId) const {
    const WeaponDef& def = GetWeaponDef(weaponId);
    auto alive = [&](int s) { return SystemIsRoom(s) && _enemy.sys[s].Installed() && _enemy.sys[s].Capacity() > 0; };

    if (_flee == FleeState::CHARGING && alive(SYS_ENGINES) && (def.missile || def.damage >= 2))
        return SYS_ENGINES;

    if (def.missile) {
        if (alive(SYS_WEAPONS)) return SYS_WEAPONS;
        if (alive(SYS_SHIELDS)) return SYS_SHIELDS;
    } else if (_enemy.sys[SYS_SHIELDS].Installed() && _enemy.sys[SYS_SHIELDS].Capacity() >= 2) {
        return SYS_SHIELDS;
    }

    for (int s : { SYS_WEAPONS, SYS_SHIELDS, SYS_ENGINES, SYS_REPAIR })
        if (alive(s)) return s;
    for (int s = 0; s < SYS_COUNT; s++)
        if (SystemIsRoom(s) && _enemy.sys[s].Installed()) return s;
    return -1;
}

void Combat::_AutoTarget(bool overwrite) {
    int assigned = 0;
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        WeaponSlot& w = _player->weapons[i];
        if (w.Empty() || !w.wanted || (!overwrite && w.target >= 0)) continue;
        int t = _BestTarget(w.id);
        if (t < 0) continue;
        _AssignTarget(i, t);
        assigned++;
    }
    _selected = -1;
    if (assigned > 0) _hud->Log(TextFormat("Ciblage auto : %d arme(s)", assigned), ui::C_ENEMY);
}

Rectangle Combat::SpeedButtonRect(int idx) {
    return { 690.0f + idx * 50.0f, 18.0f, 44.0f, 26.0f };
}

// ═════════════════════════════════════════════════════════════════════════════
// Update
// ═════════════════════════════════════════════════════════════════════════════

CombatResult Combat::Update(float dt, bool mouseConsumed) {
    if (!_active) return CombatResult::ONGOING;

    Ship& player = *_player;
    Vector2 mouse = GetMousePosition();

    // ── Entrées ──────────────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_SPACE) && !Dying()) {
        _paused = !_paused;
        _hud->Log(_paused ? "Pause tactique" : "Reprise du combat", ui::C_TEXT_DIM);
    }
    bool left = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (IsKeyPressed(KEY_C) || (left && CheckCollisionPointRec(mouse, AbilityRect(0)) && player.sys[SYS_CLOAK].Installed())) {
        _ToggleCloak();
        mouseConsumed = mouseConsumed || left;
    }
    if (IsKeyPressed(KEY_H) || (left && CheckCollisionPointRec(mouse, AbilityRect(1)) && player.sys[SYS_HACKING].Installed())) {
        _ToggleHackSelect();
        mouseConsumed = mouseConsumed || left;
    }

    bool autoClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, AutoTargetRect());
    if ((IsKeyPressed(KEY_T) || autoClick) && !Dying()) {
        _AutoTarget(true);
        mouseConsumed = mouseConsumed || autoClick;
    }
    if (IsKeyPressed(KEY_TAB)) {
        CycleSpeed();
        _hud->Log(TextFormat("Vitesse x%d", Speed()), ui::C_ACCENT);
    }
    for (int i = 0; i < 3; i++) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, SpeedButtonRect(i))) {
            _speedIdx = i;
            mouseConsumed = true;
        }
    }

    for (int i = 0; i < MAX_WEAPON_SLOTS && i < player.weaponSlots; i++) {
        if (!IsKeyPressed(KEY_ONE + i) && !IsKeyPressed(KEY_KP_1 + i)) continue;
        WeaponSlot& w = player.weapons[i];
        if (w.Empty()) continue;
        if (!w.wanted) {
            if (!player.PowerWeapon(i)) { _hud->Log("Impossible d'alimenter l'arme", ui::C_WARNING); _audio->PlaySfx("sx_error", 1.0f, 0.4f); }
        } else {
            _selected = (_selected == i) ? -1 : i;
        }
    }

    _hoverRoom = (_enemyDeathT < 0.0f) ? _renderer->RoomAt(_enemy, _enemyView, mouse) : -1;

    // Carte d'arme glissée puis lâchée sur une salle ennemie : allumer si besoin, puis viser
    {
        int slot = -1;
        Vector2 at{};
        if (_hud->TakeWeaponDrop(slot, at) && slot >= 0 && slot < MAX_WEAPON_SLOTS &&
            !player.weapons[slot].Empty() && _enemyDeathT < 0.0f && !Dying()) {
            int room = _renderer->RoomAt(_enemy, _enemyView, at);
            WeaponSlot& w = player.weapons[slot];
            if (room >= 0) {
                if (!w.wanted && !player.PowerWeapon(slot)) {
                    _hud->Log(TextFormat("%s : énergie insuffisante", GetWeaponDef(w.id).shortName), ui::C_WARNING);
                    _audio->PlaySfx("sx_error", 1.0f, 0.4f);
                } else {
                    _AssignTarget(slot, room);
                    _hud->Log(TextFormat("%s -> %s", GetWeaponDef(w.id).shortName, SystemName(room)), ui::C_ENEMY);
                    if (_selected == slot) _selected = -1;
                }
            }
        }
    }

    if (_selected >= 0) _hackSelecting = false;
    if (!mouseConsumed) {
        if (_hackSelecting && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && _hoverRoom >= 0) {
            _Hack(_hoverRoom);
        } else if (_hackSelecting && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            _hackSelecting = false;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && _selected >= 0 && _hoverRoom >= 0) {
            WeaponSlot& w = player.weapons[_selected];
            _AssignTarget(_selected, _hoverRoom);
            _hud->Log(TextFormat("%s -> %s", GetWeaponDef(w.id).shortName, SystemName(_hoverRoom)), ui::C_ENEMY);
            _selected = -1;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && _selected < 0 && _hoverRoom >= 0) {
            // Aucune arme sélectionnée : toutes les armes alimentées visent cette salle
            int n = 0;
            for (int i = 0; i < MAX_WEAPON_SLOTS; i++)
                if (!player.weapons[i].Empty() && player.weapons[i].wanted) { _AssignTarget(i, _hoverRoom); n++; }
            if (n > 0) _hud->Log(TextFormat("Toutes les armes -> %s", SystemName(_hoverRoom)), ui::C_ENEMY);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (_hoverRoom >= 0)
                for (auto& w : player.weapons)
                    if (w.target == _hoverRoom) { w.target = -1; w.preferred = -1; }
            _selected = -1;
        }
    }
    if (_selected >= 0 && (player.weapons[_selected].Empty() || !player.weapons[_selected].wanted))
        _selected = -1;

    // ── Visuel en temps réel ─────────────────────────────────────────────────
    _enemyView.pos.x += (_enemyHome.x - _enemyView.pos.x) * std::min(1.0f, dt * 2.5f);
    _enemyView.pos.y  = _enemyHome.y + std::sin((float)GetTime() * 0.8f) * 6.0f;
    for (ShipView* v : { _playerView, &_enemyView }) {
        float k = std::max(0.0f, 1.0f - 14.0f * dt);
        v->kick.x *= k;
        v->kick.y *= k;
    }
    _enemyView.thrust = (_flee == FleeState::CHARGING) ? 1.0f : (_flee == FleeState::INTENT) ? 0.4f : 0.0f;
    {
        float target = player.Cloaked() ? 0.3f : 1.0f;
        _playerView->alpha += (target - _playerView->alpha) * std::min(1.0f, dt * 8.0f);
    }
    for (auto& r : _rocks) {
        r.pos.x -= r.speed * dt * (_paused ? 0.0f : _timeScale);
        r.rot   += r.spin * dt * (_paused ? 0.0f : _timeScale);
        if (r.pos.x < -30.0f) { r.pos.x = VIRTUAL_W + 30.0f; r.pos.y = rng::Float(120.0f, 560.0f); }
    }
    for (auto& b : _beams) b.life -= dt;
    _phaseBanner = std::max(0.0f, _phaseBanner - dt);
    if (IsBoss() && _bossPhase == 3 && _enemyDeathT < 0.0f) _enemyView.thrust = std::max(_enemyView.thrust, 0.7f);
    std::erase_if(_beams, [](const Beam& b) { return b.life <= 0.0f; });

    bool idle = false;
    for (const auto& w : player.weapons)
        if (!w.Empty() && w.powered && w.charge >= 1.0f && w.target < 0) idle = true;
    _idleReady = (idle && !Dying()) ? _idleReady + dt : 0.0f;

    // Coque sous 25 %
    if (player.hull > 0 && player.hull * 4 < player.maxHull && _prevHull * 4 >= player.maxHull)
        _AutoPause("Pause auto : coque critique");
    _prevHull = player.hull;

    if (_paused) return CombatResult::ONGOING;

    // ── Échelle de temps : vitesse, ralenti de kill, gel d'impact ────────────
    _hitStop = std::max(0.0f, _hitStop - dt);
    _slowmo  = std::max(0.0f, _slowmo - dt);
    float scale = (float)Speed();
    if (_slowmo > 0.0f)  scale = std::min(scale, 0.3f);
    if (_hitStop > 0.0f) scale *= 0.04f;
    _timeScale = scale;

    // Sous-pas fixes : simulation identique en ×1 et en ×4
    float remaining = dt * scale;
    int guard = 0;
    while (remaining > 1e-5f && guard++ < 16) {
        float h = std::min(remaining, SIM_STEP);
        remaining -= h;
        CombatResult r = _Step(h);
        if (r != CombatResult::ONGOING) return r;
    }
    return CombatResult::ONGOING;
}

float Combat::_HazardInterval() const {
    switch (_hazard) {
    case BeaconHazard::ASTEROIDS:   return rng::Float(3.5f, 6.5f);
    case BeaconHazard::SOLAR_FLARE: return 22.0f;
    case BeaconHazard::ION_STORM:   return 14.0f;
    default:                  return 999.0f;
    }
}

void Combat::_TriggerHazard() {
    auto randomRoom = [](const Ship& s) {
        std::vector<int> rooms;
        for (int i = 0; i < SYS_COUNT; i++)
            if (SystemIsRoom(i) && s.sys[i].Installed()) rooms.push_back(i);
        return rooms.empty() ? -1 : rng::Pick(rooms);
    };

    switch (_hazard) {
    case BeaconHazard::ASTEROIDS: {
        // Un rocher tombe sur l'un des deux vaisseaux (boucliers et esquive s'appliquent)
        bool hitEnemy = rng::Chance(0.5f);
        Ship& target = hitEnemy ? _enemy : *_player;
        const ShipView& tv = hitEnemy ? _enemyView : *_playerView;
        int room = randomRoom(target);
        if (room < 0) break;

        Vector2 to   = _renderer->RoomPos(target, tv, room);
        Vector2 from = { tv.pos.x + rng::Float(-260.0f, 260.0f), -40.0f };
        Shot shot;
        shot.used = true; shot.fromPlayer = hitEnemy; shot.weaponId = WPN_BASIC_LASER;
        shot.targetSys = room; shot.room = to;

        int id = -1;
        for (int i = 0; i < (int)_shots.size(); i++) if (!_shots[i].used) { id = i; break; }
        if (id < 0) { _shots.push_back(shot); id = (int)_shots.size() - 1; } else _shots[id] = shot;
        if (_proj->SpawnTargeted(ProjType::ROCK, from, _ShieldEdge(target, tv, from, to), !hitEnemy, id) < 0)
            _shots[id].used = false;
        break;
    }
    case BeaconHazard::SOLAR_FLARE: {
        // Traverse les boucliers des deux camps
        for (int k = 0; k < 2; k++) {
            Ship& ship = k == 0 ? *_player : _enemy;
            ShipView& view = k == 0 ? *_playerView : _enemyView;
            int room = randomRoom(ship);
            if (room < 0) continue;
            ship.ApplyHit(room, 1, 0);
            Vector2 c = _renderer->RoomPos(ship, view, room);
            _fx->Explosion(c, 1.0f);
            _fx->Text(TextFormat("-1 %s", SystemShortName(room)), { c.x, c.y - 30 }, ui::C_MISSILE, 20, true);
        }
        _fx->Flash(0.45f, { 255, 150, 40, 255 });
        _fx->Shake(9.0f);
        _hud->Log("Éruption solaire !", ui::C_MISSILE);
        _audio->PlaySfx("missile", 0.4f, 0.8f);
        break;
    }
    case BeaconHazard::ION_STORM: {
        for (int k = 0; k < 2; k++) {
            Ship& ship = k == 0 ? *_player : _enemy;
            ShipView& view = k == 0 ? *_playerView : _enemyView;
            if (ship.sys[SYS_SHIELDS].Installed())
                ship.sys[SYS_SHIELDS].ionTimer = std::max(ship.sys[SYS_SHIELDS].ionTimer, 5.0f);
            if (ship.shieldLayers > 0) ship.shieldLayers--;
            Vector2 r = _renderer->ShieldRadii(ship);
            _fx->Ripple(view.pos, r.x, r.y, ui::C_ION);
            _fx->Spark(view.pos, ui::C_ION, 30, 220.0f, 3.0f, 0.6f);
        }
        _fx->Flash(0.2f, ui::C_ION);
        _fx->Text("IMPULSION IONIQUE", { VIRTUAL_W / 2.0f, 300 }, ui::C_ION, 20, true);
        _hud->Log("Impulsion ionique : boucliers perturbés", ui::C_ION);
        _audio->PlaySfx("laser", 0.35f, 0.6f);
        break;
    }
    default: break;
    }
}

CombatResult Combat::_Step(float h) {
    Ship& player = *_player;
    bool enemyAlive  = _enemyDeathT  < 0.0f;
    bool playerAlive = _playerDeathT < 0.0f;

    if (enemyAlive && playerAlive) {
        _elapsed += h;
        if (_hazard != BeaconHazard::NONE) {
            float regen = _hazard == BeaconHazard::ION_STORM ? 0.5f : 1.0f;
            player.shieldRegenMult = regen;
            _enemy.shieldRegenMult = regen;
            _hazardTimer -= h;
            if (_hazardTimer <= 0.0f) {
                _TriggerHazard();
                _hazardTimer = _HazardInterval();
            }
        }
    }

    player.Update(h, enemyAlive && playerAlive);
    if (enemyAlive) {
        _enemy.chargeFrozen = player.Cloaked();
        _enemy.Update(h, playerAlive);
        _EnemyAI(h);
    }

    if (enemyAlive && playerAlive) {
        _UpdateWeapons(player, _enemy, true, h);
        _UpdateWeapons(_enemy, player, false, h);
    }

    _proj->Update(h);
    _ResolveArrivals();
    if (playerAlive) _EmitDamageFx(player, *_playerView, h);
    if (enemyAlive)  _EmitDamageFx(_enemy, _enemyView, h);

    // Transitions de phase du vaisseau amiral
    if (enemyAlive && playerAlive && IsBoss() && !_enemy.Destroyed()) {
        int wanted = (_enemy.hull * 3 <= _enemy.maxHull) ? 3 : (_enemy.hull * 3 <= _enemy.maxHull * 2) ? 2 : 1;
        if (wanted > _bossPhase) _EnterBossPhase(wanted);
    }

    if (enemyAlive && playerAlive) {
        CombatResult r = _UpdateFlee(h);
        if (r != CombatResult::ONGOING) return r;
    }

    // Destructions
    if (enemyAlive && _enemy.Destroyed()) _OnEnemyDestroyed();
    if (playerAlive && player.Destroyed()) {
        _playerDeathT = 0.0f;
        _boomTimer    = 0.0f;
        _selected     = -1;
        _hitStop      = 0.15f;
        _slowmo       = 1.2f;
        _hud->Log("Coque percée !", ui::C_HULL_LOW);
        _fx->Shake(14.0f);
        _fx->Flash(0.5f, ui::C_HULL_LOW);
        _audio->PlaySfx("missile", 0.5f, 1.0f);
    }

    if (_playerDeathT >= 0.0f) {
        _UpdateDeath(_playerDeathT, player, *_playerView, h);
        if (_playerDeathT > _renderer->DestructionDuration(player) + 1.2f)
            return CombatResult::PLAYER_DESTROYED;
    } else if (_enemyDeathT >= 0.0f) {
        _UpdateDeath(_enemyDeathT, _enemy, _enemyView, h);
        if (_enemyDeathT > _renderer->DestructionDuration(_enemy) + 0.35f)
            return CombatResult::ENEMY_DESTROYED;
    }
    return CombatResult::ONGOING;
}

// Dégâts visibles : fumée sur les salles touchées, étincelles quand le système est hors service,
// coque qui fume sous 50 %. Émis en temps simulé : rien en pause, plus vite en accéléré.
void Combat::_EmitDamageFx(const Ship& ship, const ShipView& view, float h) {
    for (int s = 0; s < SYS_COUNT; s++) {
        const ShipSystem& x = ship.sys[s];
        if (!x.Installed() || x.damage <= 0 || !SystemIsRoom(s)) continue;
        Vector2 c = _renderer->RoomPos(ship, view, s);
        c.x += view.kick.x + rng::Float(-6.0f, 6.0f);
        c.y += view.kick.y + rng::Float(-6.0f, 6.0f);
        float severity = (float)x.damage / (float)x.level;
        if (rng::Chance(h * (2.0f + 4.0f * severity)))
            _fx->Smoke(c, 8.0f, 1.4f);
        if (x.damage >= x.level && rng::Chance(h * 2.5f))
            _fx->Spark(c, { 255, 160, 70, 255 }, 4, 140.0f, 2.0f, 0.3f);
    }

    float hullPct = (float)ship.hull / (float)std::max(1, ship.maxHull);
    if (hullPct < 0.5f && rng::Chance(h * (0.5f - hullPct) * 8.0f)) {
        float r = _renderer->Size(ship) * 0.25f;
        _fx->Smoke({ view.pos.x + view.kick.x + rng::Float(-r, r), view.pos.y + view.kick.y + rng::Float(-r, r) },
                   11.0f, 1.8f);
    }
}

void Combat::_EnterBossPhase(int phase) {
    _bossPhase   = phase;
    _phaseBanner = 3.2f;
    _AutoPause(phase == 2 ? "Pause auto : phase 2 du boss" : "Pause auto : phase 3 du boss");
    Ship& e = _enemy;

    // Changement de tactique (les dégâts subis par ses modules restent)
    if (phase == 2) {
        // BARRAGE : le canon ionique laisse place à des missiles, tourelle renforcée
        for (auto& w : e.weapons)
            if (!w.Empty() && w.id == WPN_ION_BLAST) { w.id = WPN_ARTEMIS; w.charge = 0.5f; }
        e.sys[SYS_DEFENSE].level = std::min(3, e.sys[SYS_DEFENSE].level + 1);
        e.chargeMult *= 1.1f;
    } else if (phase == 3) {
        // SURCHARGE : tout dans les armes et les moteurs, au détriment des boucliers
        e.chargeMult *= 1.25f;
        e.sys[SYS_SHIELDS].level = std::max(2, e.sys[SYS_SHIELDS].level - 2);
        e.sys[SYS_ENGINES].level = std::min(SystemMaxLevel(SYS_ENGINES), e.sys[SYS_ENGINES].level + 2);
    }
    e.reactor = 0;
    for (const auto& x : e.sys) e.reactor += x.level;
    e.AutoPower();
    e.shieldLayers = e.MaxShieldLayers();
    _aiTimer = 0.0f;

    Vector2 c = _enemyView.pos;
    _hitStop = 0.2f;
    _slowmo  = 1.0f;
    _fx->Flash(0.4f, ui::C_ENEMY);
    _fx->Shake(10.0f);
    _fx->Shockwave(c, _renderer->Size(e) * 0.45f, ui::C_ENEMY);
    _fx->Spark(c, ui::C_ENEMY, 60, 320.0f, 4.0f, 0.8f);
    _hud->Log(phase == 2 ? "Amiral : phase 2 — barrage de missiles !" : "Amiral : phase 3 — surcharge !", ui::C_ENEMY);
    e.RecomputePower();
    _audio->PlaySfx("sx_alarm", 0.8f, 0.8f);
}

void Combat::_OnEnemyDestroyed() {
    _enemyDeathT = 0.0f;
    _boomTimer   = 0.0f;
    _selected    = -1;
    _flee        = FleeState::NONE;

    // Moment fort : gel, ralenti, onde de choc, débris et ferraille aspirée vers le HUD
    _hitStop = 0.12f;
    _slowmo  = 0.9f;
    Vector2 c = _enemyView.pos;
    float size = _renderer->Size(_enemy);
    _fx->Shake(12.0f);
    _fx->Flash(0.35f, { 255, 170, 60, 255 });
    _fx->Shockwave(c, size * 0.35f, ui::C_ACCENT);
    _fx->Shockwave(c, size * 0.2f, WHITE);
    _fx->Debris(c, { 150, 140, 130, 255 }, 26, 320.0f);
    _fx->Debris(c, ui::C_ENEMY, 10, 260.0f);
    _fx->Pickup(c, HUD::ScrapAnchor(), ui::C_SCRAP, 22);
    _fx->Text("DÉTRUIT", { c.x, c.y - size * 0.55f }, ui::C_ACCENT, 30, true);

    _hud->Log(_enemy.name + " détruit !", ui::C_GOOD);
    _audio->PlaySfx("missile", 0.55f, 1.0f);
}

void Combat::_UpdateDeath(float& deathT, const Ship& ship, ShipView& view, float dt) {
    deathT += dt;
    _boomTimer -= dt;
    view.destroyT = deathT;

    if (deathT < 1.1f && _boomTimer <= 0.0f) {
        _boomTimer = rng::Float(0.08f, 0.18f);
        float s = _renderer->Size(ship) * 0.4f;
        Vector2 p = { view.pos.x + rng::Float(-s, s), view.pos.y + rng::Float(-s, s) };
        _fx->Explosion(p, rng::Float(0.8f, 1.5f));
        _fx->Shake(5.0f);
        _audio->PlaySfx("missile", rng::Float(0.45f, 0.7f), 0.5f);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Fuite ennemie
// ═════════════════════════════════════════════════════════════════════════════

CombatResult Combat::_UpdateFlee(float dt) {
    // Les drones, la flotte et l'amiral ne fuient jamais
    if (_kind != EnemyKind::NORMAL) return CombatResult::ONGOING;

    switch (_flee) {
    case FleeState::NONE: {
        if (_fleeTried) break;
        _fleeEval -= dt;
        if (_fleeEval > 0.0f) break;
        _fleeEval = 2.0f;

        float hp  = (float)_enemy.hull   / (float)std::max(1, _enemy.maxHull);
        float php = (float)_player->hull / (float)std::max(1, _player->maxHull);
        const ShipSystem& weapons = _enemy.sys[SYS_WEAPONS];
        bool crippled = weapons.damage * 2 >= weapons.level || _enemy.MaxShieldLayers() == 0;

        float chance = 0.0f;
        if (hp < 0.3f)                            chance = 0.40f;
        else if (hp < 0.55f && php - hp > 0.25f)  chance = 0.18f;
        else if (hp < 0.6f && crippled)           chance = 0.12f;

        if (chance > 0.0f && rng::Chance(chance)) {
            _fleeTried = true;
            _flee      = FleeState::INTENT;
            _fleeTimer = FLEE_INTENT_TIME;
            _hud->Log("L'ennemi s'apprête à fuir !", ui::C_WARNING);
            _AutoPause("Pause auto : fuite annoncée");
            _audio->PlaySfx("jump", 1.8f, 0.35f);
        }
        break;
    }

    case FleeState::INTENT:
        _fleeTimer -= dt;
        if (_fleeTimer <= 0.0f) {
            _flee       = FleeState::CHARGING;
            _fleeCharge = 0.0f;
            _aiTimer    = 0.0f;   // réaffecte l'énergie vers les moteurs tout de suite
            _hud->Log("FTL ennemi en charge : visez ses MOTEURS !", ui::C_WARNING);
        }
        break;

    case FleeState::CHARGING:
        // Un peu plus rapide que le joueur, mais bloqué si les moteurs sont hors service
        _fleeCharge += dt * _enemy.FtlChargeRate() * 1.3f;
        if (_fleeCharge >= 1.0f) {
            _fx->Flash(0.3f, ui::C_SHIELD);
            _audio->PlaySfx("jump", 1.3f, 0.6f);
            return CombatResult::ENEMY_FLED;
        }
        break;
    }
    return CombatResult::ONGOING;
}

// ═════════════════════════════════════════════════════════════════════════════
// Armes
// ═════════════════════════════════════════════════════════════════════════════

void Combat::_UpdateWeapons(Ship& shooter, Ship& target, bool fromPlayer, float dt) {
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        WeaponSlot& w = shooter.weapons[i];
        if (w.Empty()) continue;
        const WeaponDef& def = GetWeaponDef(w.id);

        // Salve en cours : la cible peut avoir été retirée ou l'arme éteinte entre deux tirs
        if (w.pendingShots > 0) {
            if (w.target < 0 || w.target >= SYS_COUNT || !target.sys[w.target].Installed()) {
                w.pendingShots = 0;
                continue;
            }
            w.burstTimer -= dt;
            if (w.burstTimer <= 0.0f) {
                _SpawnShot(fromPlayer, i);
                w.pendingShots--;
                w.burstTimer = BURST_DELAY;
            }
            continue;
        }

        if (!w.powered || w.charge < 1.0f || w.target < 0 || w.target >= SYS_COUNT) continue;
        if (!target.sys[w.target].Installed()) { w.target = -1; continue; }

        if (def.missile) {
            if (shooter.missiles <= 0) continue;
            if (shooter.HasAug(AUG_RECYCLER) && rng::Chance(0.25f)) {
                if (fromPlayer) _hud->Log("Recycleur : missile économisé", ui::C_GOOD);
            } else {
                shooter.missiles--;
            }
        }

        w.charge       = 0.0f;
        w.pendingShots = def.shots;
        w.burstTimer   = 0.0f;
        w.fireAnim     = 0.5f;
        _PlayWeaponSfx(w.id, fromPlayer);
    }
}

void Combat::_PlayWeaponSfx(int weaponId, bool fromPlayer) {
    const WeaponDef& def = GetWeaponDef(weaponId);
    float pitch  = fromPlayer ? 1.0f : 0.85f;
    float volume = fromPlayer ? 0.8f : 0.6f;
    std::string key = def.sfx;
    if (key == "gun") key = "gun_" + std::to_string(rng::Int(1, 5));
    if (def.ion > 0) pitch *= 0.7f;
    _audio->PlaySfx(key, pitch * rng::Float(0.95f, 1.05f), volume);
}

Vector2 Combat::_ShieldEdge(const Ship& target, const ShipView& view, Vector2 from, Vector2 to) const {
    Vector2 r = _renderer->ShieldRadii(target);
    float dx = to.x - from.x,        dy = to.y - from.y;
    float fx = from.x - view.pos.x,  fy = from.y - view.pos.y;
    float rx2 = r.x * r.x,           ry2 = r.y * r.y;

    float A = dx * dx / rx2 + dy * dy / ry2;
    float B = 2.0f * (fx * dx / rx2 + fy * dy / ry2);
    float C = fx * fx / rx2 + fy * fy / ry2 - 1.0f;
    float disc = B * B - 4.0f * A * C;
    if (A <= 0.0f || disc < 0.0f) return to;

    float t = (-B - std::sqrt(disc)) / (2.0f * A);
    if (t < 0.0f || t > 1.0f) return to;
    return { from.x + dx * t, from.y + dy * t };
}

void Combat::_SpawnShot(bool fromPlayer, int slot) {
    Ship&             shooter = fromPlayer ? *_player : _enemy;
    Ship&             target  = fromPlayer ? _enemy : *_player;
    ShipView&         sv      = fromPlayer ? *_playerView : _enemyView;
    const ShipView&   tv      = fromPlayer ? _enemyView : *_playerView;
    const WeaponSlot& w       = shooter.weapons[slot];
    const WeaponDef&  def     = GetWeaponDef(w.id);

    Vector2 from = _renderer->MuzzlePos(shooter, sv);
    from.y += rng::Float(-10.0f, 10.0f);
    Vector2 room = _renderer->RoomPos(target, tv, w.target);
    room.x += rng::Float(-7.0f, 7.0f);
    room.y += rng::Float(-7.0f, 7.0f);

    Shot shot;
    shot.used       = true;
    shot.fromPlayer = fromPlayer;
    shot.weaponId   = w.id;
    shot.targetSys  = w.target;
    shot.room       = room;

    int id = -1;
    for (int i = 0; i < (int)_shots.size(); i++)
        if (!_shots[i].used) { id = i; break; }
    if (id < 0) { _shots.push_back(shot); id = (int)_shots.size() - 1; }
    else        { _shots[id] = shot; }

    if (_proj->SpawnTargeted(def.proj, from, _ShieldEdge(target, tv, from, room), !fromPlayer, id) < 0) {
        _shots[id].used = false;   // plafond de projectiles atteint : tir annulé
        return;
    }

    // Recul du tireur + éclair de bouche
    float recoil = def.missile ? 9.0f : 5.0f + (float)def.damage;
    sv.kick.x += fromPlayer ? -recoil : recoil;
    Color c = fromPlayer ? ui::C_SHIELD : ui::C_ENEMY;
    _fx->Flare(from, c, def.missile ? 18.0f : 12.0f);
    _fx->Spark(from, c, 6, 110.0f, 2.0f, 0.22f);
}

void Combat::_ResolveArrivals() {
    for (auto& p : _proj->GetAllMutable()) {
        if (!p.active || !p.arrived) continue;
        if (p.shotId < 0 || p.shotId >= (int)_shots.size() || !_shots[p.shotId].used) {
            p.active = false;
            continue;
        }

        Shot& s = _shots[p.shotId];
        if (s.targetSys < 0 || s.targetSys >= SYS_COUNT) {   // garde-fou : cible invalide
            s.used = false;
            _proj->Release(p);
            continue;
        }
        Ship& target          = s.fromPlayer ? _enemy : *_player;
        ShipView& tv          = s.fromPlayer ? _enemyView : *_playerView;
        const WeaponDef& def  = GetWeaponDef(s.weaponId);
        float deathT          = s.fromPlayer ? _enemyDeathT : _playerDeathT;

        // Cible déjà détruite : le tir file dans le vide
        if (deathT >= 0.0f) {
            s.used = false;
            _proj->Release(p);
            continue;
        }

        if (s.stage == 0) {
            if (def.missile && _TryIntercept(target, tv, p)) {
                s.used   = false;
                p.active = false;
                continue;
            }
            // Esquive
            if (rng::Chance((float)target.Evasion() / 100.0f)) {
                _fx->Text("RATÉ", { p.pos.x, p.pos.y - 20 }, ui::C_TEXT_DIM, 20);
                s.used = false;
                _proj->Release(p);
                continue;
            }
            // Boucliers : les missiles passent, les armes perforantes traversent jusqu'à `pierce` couches
            if (!def.missile && target.shieldLayers > def.pierce) {
                target.shieldLayers--;
                target.shieldCharge = 0.0f;
                if (def.ion > 0)
                    target.sys[SYS_SHIELDS].ionTimer = std::max(target.sys[SYS_SHIELDS].ionTimer, 7.0f);

                Color c = def.ion > 0 ? ui::C_ION : ui::C_SHIELD;
                Vector2 r = _renderer->ShieldRadii(target);
                _fx->Ripple(tv.pos, r.x, r.y, c);
                _fx->Flare(p.pos, c, 16.0f);
                _fx->Spark(p.pos, c, 16, 200.0f, 2.5f, 0.4f);
                _fx->Text(def.ion > 0 ? "IONISÉ" : "BLOQUÉ", { p.pos.x, p.pos.y - 20 }, c, 20);
                _audio->PlaySfx("sx_shield", rng::Float(0.9f, 1.1f), s.fromPlayer ? 0.35f : 0.5f);
                if (!s.fromPlayer) _fx->Shake(1.5f);
                if (target.shieldLayers == 0) {
                    _fx->Text("BOUCLIERS TOMBÉS", { tv.pos.x, tv.pos.y - 100 }, c, 20, true);
                    _fx->Shockwave(tv.pos, r.x * 0.9f, c);
                }

                s.used   = false;
                p.active = false;
                continue;
            }
            s.stage = 1;
            _proj->Retarget(p, s.room);
            continue;
        }

        // Impact sur la salle
        int sysBefore = target.sys[s.targetSys].Capacity();
        int hullDamage = target.ApplyHit(s.targetSys, def.damage, def.ion);
        if (hullDamage < def.damage)
            _fx->Text("ABSORBÉ", { p.pos.x, p.pos.y - 52 }, ui::C_SHIELD, 20);
        bool knockedOut = sysBefore > 0 && target.sys[s.targetSys].Capacity() == 0;

        // Poussée dans le sens du tir
        float dx = p.vel.x, dy = p.vel.y, len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.0f) {
            float push = 3.0f + 3.0f * (float)def.damage;
            tv.kick.x += dx / len * push;
            tv.kick.y += dy / len * push;
        }

        if (def.damage > 0) {
            _fx->Explosion(p.pos, 0.5f + 0.3f * (float)def.damage);
            _fx->Text(TextFormat("-%d", def.damage), { p.pos.x, p.pos.y - 26 },
                      s.fromPlayer ? ui::C_WARNING : ui::C_HULL_LOW,
                      20.0f + 10.0f * (float)std::min(def.damage, 2), def.damage >= 2);
            _audio->PlaySfx("missile", rng::Float(0.7f, 0.9f), s.fromPlayer ? 0.45f : 0.6f);
            if (def.damage >= 2) _hitStop = std::max(_hitStop, 0.05f);
        }
        if (def.ion > 0) {
            _fx->Spark(p.pos, ui::C_ION, 18, 150.0f, 3.0f, 0.6f);
            _fx->Text(TextFormat("%s IONISÉ", SystemShortName(s.targetSys)), { p.pos.x, p.pos.y - 26 }, ui::C_ION, 20);
        }
        if (knockedOut) {
            _fx->Text(TextFormat("%s HS", SystemShortName(s.targetSys)), { p.pos.x, p.pos.y - 50 },
                      s.fromPlayer ? ui::C_GOOD : ui::C_HULL_LOW, 20, true);
            _hitStop = std::max(_hitStop, 0.07f);
        }
        _fx->Shake(s.fromPlayer ? 1.5f + def.damage : 3.0f + 2.5f * def.damage);
        if (!s.fromPlayer && def.damage > 0) _fx->Flash(0.08f * def.damage, ui::C_HULL_LOW);

        if (!s.fromPlayer && def.damage > 0 && target.sys[s.targetSys].damage > 0)
            _hud->Log(TextFormat("%s endommagé", SystemName(s.targetSys)), ui::C_HULL_LOW);

        s.used   = false;
        p.active = false;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// IA
// ═════════════════════════════════════════════════════════════════════════════

void Combat::_EnemyAI(float dt) {
    _aiTimer -= dt;
    if (_aiTimer > 0.0f) return;
    _aiTimer = 1.5f;

    _enemy.AutoPower();

    // En fuite : l'énergie des armes part dans les moteurs
    if (_flee == FleeState::CHARGING) {
        while (_enemy.sys[SYS_ENGINES].desired < _enemy.sys[SYS_ENGINES].Capacity()) {
            if (_enemy.AddPower(SYS_ENGINES)) continue;
            if (!_enemy.RemovePower(SYS_WEAPONS)) break;
        }
    }

    // Cibles possibles chez le joueur, boucliers privilégiés
    std::vector<int> targets;
    for (int s = 0; s < SYS_COUNT; s++) {
        if (!_player->sys[s].Installed() || !SystemIsRoom(s)) continue;
        targets.push_back(s);
        if (s == SYS_SHIELDS)              { targets.push_back(s); targets.push_back(s); }
        if (s == SYS_WEAPONS && IsBoss())  targets.push_back(s);
    }
    if (targets.empty()) return;

    for (auto& w : _enemy.weapons) {
        if (w.Empty()) continue;
        if (w.target < 0 || rng::Chance(0.2f)) {
            // Missiles et ions visent de préférence les boucliers
            const WeaponDef& def = GetWeaponDef(w.id);
            if ((def.missile || def.ion > 0) && _player->sys[SYS_SHIELDS].Installed() && rng::Chance(0.6f))
                w.target = SYS_SHIELDS;
            else
                w.target = rng::Pick(targets);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Dessin
// ═════════════════════════════════════════════════════════════════════════════

void Combat::DrawBackdrop(float t) const {
    if (!_active) return;
    switch (_hazard) {
    case BeaconHazard::ASTEROIDS:
        for (const auto& r : _rocks) {
            Color c = ColorAlpha({ 90, 84, 78, 255 }, 0.35f + r.size / 40.0f);
            DrawPoly(r.pos, 6, r.size, r.rot, c);
            DrawPolyLinesEx(r.pos, 6, r.size, r.rot, 1.0f, ColorAlpha({ 150, 140, 130, 255 }, 0.5f));
        }
        break;
    case BeaconHazard::SOLAR_FLARE: {
        // Soleil hors champ : halo orange qui s'intensifie avant l'éruption
        float warn = std::clamp(1.0f - _hazardTimer / 4.0f, 0.0f, 1.0f);
        float pulse = 0.5f + 0.5f * std::sin(t * (3.0f + warn * 12.0f));
        DrawRectangleGradientV(0, 0, VIRTUAL_W, 260, ColorAlpha({ 255, 120, 30, 255 }, 0.10f + 0.25f * warn * pulse), BLANK);
        break;
    }
    case BeaconHazard::ION_STORM:
        for (int i = 0; i < 6; i++) {
            float x = std::fmod(t * 90.0f + i * 223.0f, (float)VIRTUAL_W);
            if (std::fmod(t * 7.0f + i, 3.0f) < 0.15f)
                DrawLineEx({ x, 0 }, { x + 40.0f, (float)VIRTUAL_H }, 2.0f, ColorAlpha(ui::C_ION, 0.35f));
        }
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(ui::C_ION, 0.04f));
        break;
    default: break;
    }
}

void Combat::DrawWorld(float t) const {
    if (!_active) return;

    _renderer->DrawShip(_enemy, _enemyView, t);
    _renderer->DrawShield(_enemy, _enemyView, t);

    int mask[SYS_COUNT] = {};
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        const WeaponSlot& w = _player->weapons[i];
        if (!w.Empty() && w.target >= 0 && w.target < SYS_COUNT) mask[w.target] |= (1 << i);
    }
    int hovered = (_selected >= 0 || _hud->DraggedWeapon() >= 0) ? _hoverRoom : -1;
    _renderer->DrawRooms(_enemy, _enemyView, true, hovered, mask, t);
    _renderer->DrawRooms(*_player, *_playerView, false, -1, nullptr, t);

    // Salles du joueur visées par l'ennemi : plus l'arme est chargée, plus le marqueur est vif
    if (_enemyDeathT < 0.0f && _playerDeathT < 0.0f) {
        for (const auto& w : _enemy.weapons) {
            if (w.Empty() || !w.powered || w.target < 0 || w.target >= SYS_COUNT) continue;
            if (!_player->sys[w.target].Installed()) continue;
            Vector2 c = _renderer->RoomPos(*_player, *_playerView, w.target);
            c.x += _playerView->kick.x;
            c.y += _playerView->kick.y;
            float danger = w.charge;
            bool imminent = danger > 0.85f && std::fmod(t, 0.3f) < 0.15f;
            float s = 22.0f + 6.0f * (1.0f - danger);
            ui::CornerBrackets({ c.x - s, c.y - s, s * 2, s * 2 }, 6.0f,
                               ColorAlpha(ui::C_ENEMY, imminent ? 1.0f : 0.25f + 0.5f * danger));
        }
    }

    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& b : _beams) {
        float a = std::clamp(b.life * 4.0f, 0.0f, 1.0f);
        DrawLineEx(b.from, b.to, 6.0f, ColorAlpha(b.color, a * 0.3f));
        DrawLineEx(b.from, b.to, 2.0f, ColorAlpha(WHITE, a));
    }
    EndBlendMode();

    // Intention de fuite affichée au-dessus de l'ennemi
    if (_flee != FleeState::NONE && _enemyDeathT < 0.0f) {
        bool blink = std::fmod(t, 0.5f) < 0.3f;
        Vector2 c = _enemyView.pos;
        float top = c.y - _renderer->Size(_enemy) * 0.5f - 40.0f;
        const char* msg = _flee == FleeState::INTENT
                        ? TextFormat("FUITE DANS %.0fs", std::ceil(_fleeTimer))
                        : "FTL EN CHARGE";
        float w = ui::MeasureLabel(msg, 20) + 20;
        DrawRectangleRec({ c.x - w / 2, top, w, 26 }, blink ? ui::C_WARNING : ColorAlpha(ui::C_INK, 0.8f));
        ui::LabelCentered(msg, c.x, top + 4, 20, blink ? ui::C_INK : ui::C_WARNING);
        if (_flee == FleeState::CHARGING) {
            DrawRectangleRec({ c.x - w / 2, top + 28, w, 5 }, ColorAlpha(ui::C_INK, 0.8f));
            DrawRectangleRec({ c.x - w / 2, top + 28, w * _fleeCharge, 5 }, ui::C_WARNING);
        }
    }
}

void Combat::DrawOverlay(float t) const {
    if (!_active) return;

    // Vitesse de jeu + chronomètre
    ui::Console({ SpeedButtonRect(0).x - 6, 12, 162, 66 }, ui::C_ACCENT);
    for (int i = 0; i < 3; i++) {
        Rectangle r = SpeedButtonRect(i);
        bool on    = i == _speedIdx;
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        ui::Bevel(r, 5, on ? ui::C_ACCENT : ColorAlpha(ui::C_INK, hover ? 0.8f : 0.5f));
        ui::BevelLines(r, 5, on ? ui::C_ACCENT : ColorAlpha(ui::C_ACCENT, hover ? 0.9f : 0.4f));
        ui::LabelCentered(TextFormat("x%d", SPEEDS[i]), r.x + r.width / 2, r.y + 8, 10, on ? ui::C_INK : ui::C_TEXT);
    }
    int secs = (int)_elapsed;
    ui::Label(TextFormat("T+%02d:%02d", secs / 60, secs % 60), SpeedButtonRect(0).x, 52, 20,
              _paused ? ui::C_WARNING : ui::C_TEXT);
    ui::Label("[TAB]", SpeedButtonRect(2).x + 2, 56, 10, ui::C_TEXT_DIM);

    if (_hazard != BeaconHazard::NONE) {
        Color c = _hazard == BeaconHazard::ION_STORM ? ui::C_ION : _hazard == BeaconHazard::SOLAR_FLARE ? ui::C_MISSILE : ui::C_LABEL;
        float x = SpeedButtonRect(0).x;
        DrawPoly({ x + 6, 90 }, 4, 6.0f, 0.0f, c);
        ui::Label(HazardName(_hazard), x + 18, 85, 10, c);
        if (_hazard != BeaconHazard::ASTEROIDS && _enemyDeathT < 0.0f && _playerDeathT < 0.0f) {
            float interval = _hazard == BeaconHazard::SOLAR_FLARE ? 22.0f : 14.0f;
            DrawRectangleRec({ x, 100, 140, 4 }, ColorAlpha(BLACK, 0.5f));
            DrawRectangleRec({ x, 100, 140 * (1.0f - std::clamp(_hazardTimer / interval, 0.0f, 1.0f)), 4 }, c);
        }
        if (_hazard == BeaconHazard::SOLAR_FLARE && _hazardTimer < 4.0f && _enemyDeathT < 0.0f && _playerDeathT < 0.0f) {
            bool blink = std::fmod(t, 0.4f) < 0.25f;
            ui::LabelCentered(TextFormat("ÉRUPTION DANS %.0f", std::ceil(_hazardTimer)), VIRTUAL_W / 2.0f, 196, 30,
                              blink ? ui::C_MISSILE : ui::C_WARNING);
        }
    }

    // Capacités actives
    for (int i = 0; i < 2; i++) {
        int sys = i == 0 ? SYS_CLOAK : SYS_HACKING;
        const Ship& p = *_player;
        if (!p.sys[sys].Installed()) continue;
        Rectangle r = AbilityRect(i);
        bool powered = p.sys[sys].power > 0;
        bool active  = i == 0 ? p.Cloaked() : _hackSelecting;
        float cd     = i == 0 ? p.cloakCooldown : p.hackCooldown;
        bool ready   = powered && cd <= 0.0f && !active;
        Color c      = i == 0 ? ui::C_SHIELD : ui::C_ION;
        bool hover   = CheckCollisionPointRec(GetMousePosition(), r);

        ui::Bevel(r, 6, ColorAlpha(ui::C_INK, 0.7f));
        if (i == 0 && p.Cloaked())
            DrawRectangleRec({ r.x + 6, r.y + 2, (r.width - 12) * (p.cloakTimer / p.CloakDuration()), r.height - 4 }, ColorAlpha(c, 0.45f));
        else if (cd > 0.0f)
            DrawRectangleRec({ r.x + 6, r.y + r.height - 5, (r.width - 12) * (1.0f - std::min(1.0f, cd / 22.0f)), 3 }, ColorAlpha(c, 0.6f));
        if (active && i == 1) ui::Bevel(r, 6, ColorAlpha(c, 0.3f + 0.2f * std::sin(t * 10.0f)));
        ui::BevelLines(r, 6, ColorAlpha(c, powered ? (hover ? 1.0f : 0.8f) : 0.35f), ready || active ? 2.0f : 1.0f);

        ui::DrawIcon(sys, { r.x + 16, r.y + r.height / 2 }, 1.0f, powered ? c : ui::C_TEXT_DIM);
        ui::Label(i == 0 ? "INVIS. [C]" : "PIRATAGE [H]", r.x + 30, r.y + 5, 10, powered ? ui::C_TEXT : ui::C_TEXT_DIM);
        const char* st = !powered ? "HORS TENSION"
                       : (i == 0 && p.Cloaked()) ? TextFormat("ACTIF %.0fs", p.cloakTimer)
                       : active ? "CHOISIR SALLE"
                       : cd > 0.0f ? TextFormat("RECHARGE %.0fs", cd) : "PRÊT";
        ui::Label(st, r.x + 30, r.y + 17, 10, ready ? c : ui::C_TEXT_DIM);
    }
    if (_hackSelecting) {
        Vector2 m = GetMousePosition();
        float r = 14.0f;
        DrawRectangleLinesEx({ m.x - r, m.y - r, r * 2, r * 2 }, 2.0f, ui::C_ION);
        ui::Label(_hoverRoom >= 0 ? SystemName(_hoverRoom) : "PIRATER ?", m.x + r + 8, m.y + 4, 10, ui::C_ION);
    }

    {
        Rectangle r = AutoTargetRect();
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        bool nag   = _idleReady > 2.5f && std::fmod(t, 0.6f) < 0.35f;
        ui::Bevel({ r.x + 3, r.y + 3, r.width, r.height }, 7, ColorAlpha(BLACK, 0.35f));
        ui::Bevel(r, 7, hover || nag ? ui::C_ENEMY : ColorAlpha(ui::C_INK, 0.7f));
        ui::BevelLines(r, 7, ui::C_ENEMY, hover || nag ? 2.0f : 1.0f);
        ui::DrawIcon(ui::ICON_WEAPONS, { r.x + 18, r.y + r.height / 2 }, 1.0f, hover || nag ? ui::C_INK : ui::C_ENEMY);
        ui::LabelCentered("AUTO [T]", r.x + r.width / 2 + 10, r.y + 10, 10, hover || nag ? ui::C_INK : ui::C_TEXT);
    }
    if (_idleReady > 2.5f && !_paused) {
        const char* msg = "Arme prête sans cible : [T] ciblage auto, ou clic sur une salle ennemie";
        float w = ui::Measure(msg, 15) + 28;
        Rectangle r = { (VIRTUAL_W - w) / 2.0f, 236, w, 28 };
        ui::Bevel(r, 6, ColorAlpha(ui::C_INK, 0.85f));
        DrawRectangleRec({ r.x + 2, r.y + 6, 3, r.height - 12 }, ui::C_ENEMY);
        ui::Text(msg, r.x + 14, r.y + 6, 15, std::fmod(t, 0.8f) < 0.5f ? ui::C_TEXT : ui::C_ENEMY);
    }

    // Phases du boss
    if (IsBoss() && _enemyDeathT < 0.0f) {
        Vector2 c = _enemyView.pos;
        float top = c.y - _renderer->Size(_enemy) * 0.5f - 34.0f;
        const char* label = TextFormat("AMIRAL - PHASE %d/3", _bossPhase);
        float w = ui::MeasureLabel(label, 10) + 16;
        DrawRectangleRec({ c.x - w / 2, top, w, 16 }, ui::C_ENEMY);
        ui::LabelCentered(label, c.x, top + 3, 10, ui::C_INK);
        // Seuils de phase sur la barre de coque ennemie
        for (int k = 1; k <= 2; k++) {
            float x = (VIRTUAL_W - 18.0f - 360.0f) + 360.0f * (float)k / 3.0f;
            DrawRectangleRec({ x - 1, 38, 2, 20 }, ui::C_TEXT);
        }
    }
    if (_phaseBanner > 0.0f) {
        float a = std::min(1.0f, _phaseBanner * 2.0f);
        const char* title = _bossPhase == 2 ? "PHASE 2 : BARRAGE" : "PHASE 3 : SURCHARGE";
        const char* sub   = _bossPhase == 2 ? "Missiles et tourelle anti-missile renforcés"
                                            : "Armes et moteurs surchargés, boucliers affaiblis";
        Rectangle band = { 0, 250, (float)VIRTUAL_W, 90 };
        DrawRectangleRec(band, ColorAlpha(ui::C_INK, 0.75f * a));
        ui::Hazard({ 0, band.y, (float)VIRTUAL_W, 8 }, ColorAlpha(ui::C_ENEMY, a), t * 60.0f);
        ui::Hazard({ 0, band.y + band.height - 8, (float)VIRTUAL_W, 8 }, ColorAlpha(ui::C_ENEMY, a), -t * 60.0f);
        ui::LabelCentered(title, VIRTUAL_W / 2.0f, band.y + 20, 40, ColorAlpha(ui::C_ENEMY, a));
        ui::TextCentered(sub, VIRTUAL_W / 2.0f, band.y + 62, 16, ColorAlpha(ui::C_TEXT, a));
    }

    if (_paused) {
        bool blink = std::fmod(t, 1.0f) < 0.6f;
        DrawRectangleLinesEx({ 0, 0, (float)VIRTUAL_W, (float)VIRTUAL_H }, 3.0f, ColorAlpha(ui::C_WARNING, 0.7f));
        const char* msg = "|| PAUSE TACTIQUE - [ESPACE]";
        float w = ui::MeasureLabel(msg, 20) + 40;
        Rectangle r = { (VIRTUAL_W - w) / 2.0f, 150, w, 30 };
        ui::Bevel(r, 8, ColorAlpha(ui::C_WARNING, blink ? 1.0f : 0.8f));
        ui::LabelCentered(msg, VIRTUAL_W / 2.0f, r.y + 5, 20, ui::C_INK);
    } else if (Speed() > 1 && _slowmo <= 0.0f) {
        ui::LabelCentered(TextFormat(">> x%d", Speed()), VIRTUAL_W / 2.0f, 156, 20, ColorAlpha(ui::C_ACCENT, 0.7f));
    }

    Vector2 m = GetMousePosition();

    // Info-bulle sur les salles ennemies (tampons fixes : aucune allocation par image)
    if (_hoverRoom >= 0 && _hoverRoom < SYS_COUNT && _enemyDeathT < 0.0f) {
        const ShipSystem& x = _enemy.sys[_hoverRoom];
        int dragged = _hud->DraggedWeapon();
        if (dragged >= MAX_WEAPON_SLOTS || (dragged >= 0 && _player->weapons[dragged].Empty())) dragged = -1;
        int evasion = std::clamp(_enemy.Evasion(), 0, 100);

        char line[96], odds[128], aimed[96], hint[96];
        std::snprintf(line, sizeof line, "Énergie %d/%d%s%s%s", x.power, x.level,
                      x.damage > 0 ? TextFormat(" · %d endommagée(s)", x.damage) : "",
                      x.ionTimer > 0.0f ? " · ionisé" : "", x.hackTimer > 0.0f ? " · piraté" : "");

        // Aperçu du tir : esquive et boucliers face à l'arme concernée
        int focus = dragged >= 0 ? dragged : _selected;
        const WeaponDef* fd = (focus >= 0 && focus < MAX_WEAPON_SLOTS && !_player->weapons[focus].Empty())
                            ? &GetWeaponDef(_player->weapons[focus].id) : nullptr;
        int layers = _enemy.shieldLayers;
        if (fd && fd->missile)
            std::snprintf(odds, sizeof odds, "Toucher %d%% · traverse les boucliers", 100 - evasion);
        else if (fd && layers > 0 && layers <= fd->pierce)
            std::snprintf(odds, sizeof odds, "Toucher %d%% · perce %d bouclier(s)", 100 - evasion, layers);
        else if (fd && layers > 0)
            std::snprintf(odds, sizeof odds, "Toucher %d%% · %d bouclier(s) : %s", 100 - evasion, layers,
                          fd->shots > layers ? "salve suffisante" : "salve bloquée");
        else if (fd)
            std::snprintf(odds, sizeof odds, "Toucher %d%% · boucliers à terre", 100 - evasion);
        else if (layers > 0)
            std::snprintf(odds, sizeof odds, "Toucher %d%% · %d bouclier(s)", 100 - evasion, layers);
        else
            std::snprintf(odds, sizeof odds, "Toucher %d%%", 100 - evasion);

        size_t used = 0;
        aimed[0] = '\0';
        for (int i = 0; i < MAX_WEAPON_SLOTS && used < sizeof aimed; i++) {
            const WeaponSlot& w = _player->weapons[i];
            if (w.Empty() || w.target != _hoverRoom) continue;
            int n = std::snprintf(aimed + used, sizeof aimed - used, "%s%s",
                                  used == 0 ? "Visée par : " : ", ", GetWeaponDef(w.id).shortName);
            if (n < 0) break;
            used = std::min(sizeof aimed - 1, used + (size_t)n);
        }

        if (dragged >= 0)
            std::snprintf(hint, sizeof hint, "Relâcher : %s vise ici", GetWeaponDef(_player->weapons[dragged].id).shortName);
        else
            std::snprintf(hint, sizeof hint, "%s", _selected >= 0 ? "Clic : assigner l'arme sélectionnée"
                                                                  : "Clic : TOUTES les armes visent ici");

        bool hasAimed = aimed[0] != '\0';
        float w = std::max({ ui::Measure(line, 15), ui::Measure(odds, 15),
                             hasAimed ? ui::Measure(aimed, 14) : 0.0f, ui::Measure(hint, 14) }) + 28;
        w = std::max(w, ui::MeasureLabel(SystemName(_hoverRoom), 20) + 28);
        float h = 92.0f + (hasAimed ? 18.0f : 0.0f);
        w += 24;
        Rectangle r = { std::min(m.x + 26, (float)VIRTUAL_W - w - 8), std::min(m.y + 24, (float)VIRTUAL_H - h - 8), w, h };
        ui::Bevel({ r.x + 3, r.y + 3, r.width, r.height }, 8, ColorAlpha(BLACK, 0.4f));
        ui::Bevel(r, 8, ColorAlpha(ui::C_INK, 0.9f));
        ui::BevelLines(r, 8, ui::C_ENEMY);
        DrawRectangleRec({ r.x + 8, r.y, r.width - 8, 2 }, ui::C_ENEMY);
        ui::DrawIcon(_hoverRoom, { r.x + 22, r.y + 20 }, 1.0f, ui::C_ENEMY);
        ui::Label(SystemName(_hoverRoom), r.x + 38, r.y + 10, 20, ui::C_TEXT);
        float ly = r.y + 34;
        ui::Text(line, r.x + 14, ly, 15, ui::C_LABEL);  ly += 18;
        ui::Text(odds, r.x + 14, ly, 15, ui::C_LABEL);  ly += 18;
        if (hasAimed) { ui::Text(aimed, r.x + 14, ly, 14, ui::C_ENEMY); ly += 18; }
        ui::Text(hint, r.x + 14, ly, 14, (_selected >= 0 || dragged >= 0) ? ui::C_ENEMY : ui::C_TEXT_DIM);
    }

    // Curseur de visée
    if (_selected >= 0) {
        Color c = _hoverRoom >= 0 ? ui::C_ENEMY : ColorAlpha(ui::C_ENEMY, 0.7f);
        float r = 12.0f + 2.0f * std::round(std::sin(t * 8.0f));
        DrawRectangleLinesEx({ m.x - r, m.y - r, r * 2, r * 2 }, 2.0f, c);
        DrawRectangleRec({ m.x - r - 8, m.y - 1, 10, 2 }, c);
        DrawRectangleRec({ m.x + r - 2, m.y - 1, 10, 2 }, c);
        DrawRectangleRec({ m.x - 1, m.y - r - 8, 2, 10 }, c);
        DrawRectangleRec({ m.x - 1, m.y + r - 2, 2, 10 }, c);
        if (_hoverRoom < 0) {
            const WeaponDef& d = GetWeaponDef(_player->weapons[_selected].id);
            ui::Label(d.shortName, m.x + r + 12, m.y + 6, 10, c);
        }
    }
}
