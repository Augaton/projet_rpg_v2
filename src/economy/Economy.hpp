#pragma once

class Economy {
public:
    int credits = 500;

    bool CanAfford(int cost) const noexcept { return credits >= cost; }

    bool Buy(int cost) noexcept {
        if (!CanAfford(cost)) return false;
        credits -= cost;
        return true;
    }

    void Earn(int amount) noexcept { credits += amount; }
};
