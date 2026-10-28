#pragma once
#include "raylib.h"
#include "../map/SectorMap.hpp"
#include "../ship/Ship.hpp"
#include "../ship/ShipFactory.hpp"
#include "../ship/ShipRenderer.hpp"
#include <vector>

class AudioManager;
class ProjectileManager;
class Effects;
class HUD;
struct Projectile;

enum class CombatResult {
    ONGOING,
    ENEMY_DESTROYED,
    ENEMY_FLED,
    PLAYER_DESTROYED,
};

// Intention de fuite de l'ennemi, toujours annoncée à l'avance
enum class FleeState {
    NONE,
    INTENT,     // l'ennemi annonce qu'il va fuir (compte à rebours)
    CHARGING,   // son FTL charge : détruire ses moteurs l'en empêche
};

class Combat {
public:
    static constexpr float FLEE_INTENT_TIME = 6.0f;

    void Init(AudioManager* audio, ProjectileManager* proj, Effects* fx,
              const ShipRenderer* renderer, HUD* hud);

    void Begin(Ship* player, ShipView* playerView, const Ship& enemy, EnemyKind kind,
               BeaconHazard hazard = BeaconHazard::NONE);
    void End();

    // mouseConsumed : la souris est déjà utilisée par le HUD ce frame
    CombatResult Update(float dt, bool mouseConsumed);

    void DrawBackdrop(float t) const;   // décor du danger, derrière les vaisseaux
    void DrawWorld(float t) const;
    void DrawOverlay(float t) const;

    bool      Paused() const        { return _paused; }
    bool      Dying() const         { return _enemyDeathT >= 0.0f || _playerDeathT >= 0.0f; }
    bool      IsBoss() const        { return _kind == EnemyKind::BOSS; }
    int*      SelectedWeapon()      { return &_selected; }
    int       Selected() const      { return _selected; }
    bool      CancelTargeting();
    // Pause automatique aux moments clés (début, fuite annoncée, coque critique, phase du boss)
    void      SetAutoPause(bool on) { _autoPause = on; }
    FleeState Flee() const          { return _flee; }
    // INTENT : secondes restantes ; CHARGING : progression 0..1
    float     FleeValue() const;
    const Ship& Enemy() const       { return _enemy; }

    // Vitesse de jeu (×1, ×2, ×4) et échelle de temps effective (ralenti, gel d'impact)
    int       Speed() const         { return SPEEDS[_speedIdx]; }
    void      CycleSpeed()          { _speedIdx = (_speedIdx + 1) % 3; }
    float     TimeScale() const     { return _paused ? 0.0f : _timeScale; }

    static Rectangle SpeedButtonRect(int idx);
    static Rectangle AutoTargetRect();
    static Rectangle AbilityRect(int idx);   // 0 = invisibilité, 1 = piratage

private:
    static constexpr int SPEEDS[3] = { 1, 2, 4 };

    struct Shot {
        bool    used       = false;
        bool    fromPlayer = false;
        int     weaponId   = 0;
        int     targetSys  = 0;
        int     stage      = 0;     // 0 = vers le bouclier, 1 = vers la salle
        Vector2 room       = { 0, 0 };
    };

    AudioManager*       _audio    = nullptr;
    ProjectileManager*  _proj     = nullptr;
    Effects*            _fx       = nullptr;
    const ShipRenderer* _renderer = nullptr;
    HUD*                _hud      = nullptr;

    Ship*     _player     = nullptr;
    ShipView* _playerView = nullptr;
    Ship      _enemy;
    ShipView  _enemyView;
    Vector2   _enemyHome  = { 0, 0 };
    EnemyKind _kind       = EnemyKind::NORMAL;

    std::vector<Shot> _shots;

    bool  _active       = false;
    bool  _paused       = false;
    bool  _autoPause    = false;
    int   _prevHull     = 0;
    int   _selected     = -1;
    int   _hoverRoom    = -1;
    float _aiTimer      = 0.0f;
    float _enemyDeathT  = -1.0f;
    float _playerDeathT = -1.0f;
    float _boomTimer    = 0.0f;

    // Temps
    int   _speedIdx  = 0;
    float _timeScale = 1.0f;
    float _elapsed   = 0.0f;
    float _hitStop   = 0.0f;
    float _slowmo    = 0.0f;

    // Fuite
    FleeState _flee       = FleeState::NONE;
    float     _fleeTimer  = 0.0f;
    float     _fleeCharge = 0.0f;
    float     _fleeEval   = 3.0f;
    bool      _fleeTried  = false;

    float     _idleReady  = 0.0f;   // temps passé avec une arme prête sans cible
    bool      _hackSelecting = false;

    // Boss : 3 phases (66 % et 33 % de coque)
    int       _bossPhase   = 1;
    float     _phaseBanner = 0.0f;
    void      _EnterBossPhase(int phase);

    // Danger de la balise
    BeaconHazard    _hazard      = BeaconHazard::NONE;
    float     _hazardTimer = 0.0f;
    struct Rock {
        Vector2 pos;
        float   size, rot, spin, speed;
    };
    std::vector<Rock> _rocks;
    void  _TriggerHazard();
    float _HazardInterval() const;

    struct Beam {
        Vector2 from, to;
        Color   color;
        float   life;
    };
    std::vector<Beam> _beams;

    void _ToggleCloak();
    void _ToggleHackSelect();
    void _AutoPause(const char* reason);
    void _Hack(int sys);
    bool _TryIntercept(Ship& target, const ShipView& tv, Projectile& p);

    CombatResult _Step(float h);
    int  _BestTarget(int weaponId) const;
    void _AutoTarget(bool overwrite);
    void _AssignTarget(int slot, int sys);
    void _UpdateWeapons(Ship& shooter, Ship& target, bool fromPlayer, float dt);
    void _SpawnShot(bool fromPlayer, int slot);
    void _ResolveArrivals();
    void _EnemyAI(float dt);
    CombatResult _UpdateFlee(float dt);
    void _UpdateDeath(float& deathT, const Ship& ship, ShipView& view, float dt);
    void _EmitDamageFx(const Ship& ship, const ShipView& view, float h);
    void _OnEnemyDestroyed();
    void _PlayWeaponSfx(int weaponId, bool fromPlayer);
    Vector2 _ShieldEdge(const Ship& target, const ShipView& view, Vector2 from, Vector2 to) const;
};
