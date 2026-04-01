#pragma once
#include "raylib.h"
#include "../inventory/Item.hpp"
#include "../inventory/Inventory.hpp"
#include "../economy/Economy.hpp"
#include "../ship/ShipStats.hpp"
#include <vector>

class Shop {
public:
    bool isOpen = false;

    std::vector<Item> stock;

    void GenerateStock(int zoneLevel);

    void Open()  { isOpen = true;  _tab = 0; _selectedIdx = -1; }
    void Close() { isOpen = false; }

    // Retourne true si l'achat/vente a modifié les stats (mod équipé)
    void Update(Vector2 mousePos, Economy& eco, Inventory& inv,
                ShipStats& stats, int screenW, int screenH);

    void Draw(Font font, const Economy& eco, const Inventory& inv,
              int screenW, int screenH) const;

private:
    int _tab         = 0;  // 0 = BUY | 1 = SELL
    int _selectedIdx = -1;

    void _DrawItemRow(Font font, const Item& item,
                      float x, float y, float w, float h,
                      bool hovered, bool canAfford) const;
};
