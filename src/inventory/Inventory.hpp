#pragma once
#include "Item.hpp"
#include <vector>
#include <algorithm>

class Inventory {
public:
    static constexpr int MAX_SLOTS = 20;

    std::vector<Item> items;
    int equippedWeaponIdx = -1;
    int equippedShieldIdx = -1;
    int equippedHullIdx   = -1;

    void AddItem(const Item& item) {
        if ((int)items.size() < MAX_SLOTS)
            items.push_back(item);
    }

    bool RemoveAt(int idx) {
        if (idx < 0 || idx >= (int)items.size()) return false;
        auto fix = [&](int& slot) {
            if (slot == idx) slot = -1;
            else if (slot > idx) slot--;
        };
        fix(equippedWeaponIdx);
        fix(equippedShieldIdx);
        fix(equippedHullIdx);
        items.erase(items.begin() + idx);
        return true;
    }

    void EquipWeapon(int idx) {
        if (idx >= 0 && idx < (int)items.size() &&
            items[idx].category == ItemCategory::WEAPON)
            equippedWeaponIdx = idx;
    }
    void UnequipWeapon()            { equippedWeaponIdx = -1; }

    void EquipShield(int idx) {
        if (idx >= 0 && idx < (int)items.size() &&
            items[idx].category == ItemCategory::SHIELD_MOD)
            equippedShieldIdx = idx;
    }
    void UnequipShield()            { equippedShieldIdx = -1; }

    void EquipHull(int idx) {
        if (idx >= 0 && idx < (int)items.size() &&
            items[idx].category == ItemCategory::HULL_MOD)
            equippedHullIdx = idx;
    }
    void UnequipHull()              { equippedHullIdx = -1; }

    const Item* GetEquippedWeapon() const {
        if (equippedWeaponIdx < 0 || equippedWeaponIdx >= (int)items.size())
            return nullptr;
        return &items[equippedWeaponIdx];
    }
    const Item* GetEquippedShield() const {
        if (equippedShieldIdx < 0 || equippedShieldIdx >= (int)items.size())
            return nullptr;
        return &items[equippedShieldIdx];
    }
    const Item* GetEquippedHull() const {
        if (equippedHullIdx < 0 || equippedHullIdx >= (int)items.size())
            return nullptr;
        return &items[equippedHullIdx];
    }

    bool IsFull() const { return (int)items.size() >= MAX_SLOTS; }
};
