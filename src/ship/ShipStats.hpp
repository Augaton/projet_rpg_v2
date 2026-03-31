#pragma once

struct ShipStats {
    // Valeurs courantes
    float hull;
    float shield;
    float fuel;

    // Maximums
    float maxHull;
    float maxShield;
    float maxFuel;

    // Régénération bouclier (unités/seconde)
    float shieldRegenRate;
    float shieldRegenDelay;     // Délai après un impact avant regen
    float shieldRegenTimer;     // Timer interne

        ShipStats()
                : hull(80), shield(60), fuel(14),
                    maxHull(100), maxShield(100), maxFuel(20),
                    shieldRegenRate(4.0f),
                    shieldRegenDelay(3.0f),
                    shieldRegenTimer(0.0f)
        {}

    void TakeDamage(float amount) {
        shieldRegenTimer = shieldRegenDelay; // Reset le délai de regen

        if (shield > 0) {
            shield -= amount;
            if (shield < 0) {
                hull  += shield; // Le débordement passe sur la coque
                shield = 0;
            }
        } else {
            hull -= amount;
        }
        if (hull < 0) hull = 0;
    }

    void Update(float dt) {
        // Regen bouclier après délai
        if (shieldRegenTimer > 0) {
            shieldRegenTimer -= dt;
        } else if (shield < maxShield) {
            shield += shieldRegenRate * dt;
            if (shield > maxShield) shield = maxShield;
        }
    }

    float HullPct()   const { return hull   / maxHull;   }
    float ShieldPct() const { return shield / maxShield; }
    float FuelPct()   const { return fuel   / maxFuel;   }
};