#pragma once
#include "Item.hpp"
#include <vector>
#include <algorithm>

class Inventory {
public:
    static constexpr int MAX_SLOTS = 20;

    std::vector<Item> items;
    int equippedWeaponIdx = -1;

    void AddItem(const Item& item) {
        if ((int)items.size() < MAX_SLOTS)
            items.push_back(item);
    }

    bool RemoveAt(int idx) {
        if (idx < 0 || idx >= (int)items.size()) return false;
        if (idx == equippedWeaponIdx)       equippedWeaponIdx = -1;
        else if (equippedWeaponIdx > idx)   equippedWeaponIdx--;
        items.erase(items.begin() + idx);
        return true;
    }

    void EquipWeapon(int idx) {
        if (idx >= 0 && idx < (int)items.size() &&
            items[idx].category == ItemCategory::WEAPON)
            equippedWeaponIdx = idx;
    }

    const Item* GetEquippedWeapon() const {
        if (equippedWeaponIdx < 0 || equippedWeaponIdx >= (int)items.size())
            return nullptr;
        return &items[equippedWeaponIdx];
    }

    bool IsFull() const { return (int)items.size() >= MAX_SLOTS; }
};
