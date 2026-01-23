#include "pch.h"
#include "Inventory.h"
#include <FontSystem.h>

namespace Framework {

    Inventory& Inventory::Instance() {
        static Inventory g;
        return g;
    }

    int Inventory::AddFirstEmpty(int itemId, GLuint iconTex, int count) {
        for (int i = 0; i < kHotbarSlots; ++i) {
            if (m_hotbar[i].itemId == (unsigned)-1) {
                m_hotbar[i] = { itemId, iconTex, count };
                return i;
            }
        }
        return -1; // no space
    }

    void Inventory::SetHotbarSlot(int idx, int itemId, GLuint iconTex, int count) {
        if (idx < 0 || idx >= kHotbarSlots) return;
        m_hotbar[idx] = { itemId, iconTex, count };
    }

    void Inventory::ClearHotbarSlot(int idx) {
        if (idx < 0 || idx >= kHotbarSlots) return;
        m_hotbar[idx] = {};
        m_hotbar[idx].itemId = (unsigned)-1;
    }

    void Inventory::SetSelectedIndex(int idx) {
        if (idx < 0) idx = 0;
        if (idx >= kHotbarSlots) idx = kHotbarSlots - 1;
        m_selectedIndex = idx;
    }

    void Inventory::ChangeSelected(int delta) {
        delta = -delta;
        int i = (m_selectedIndex + delta) % kHotbarSlots;
        if (i < 0) i += kHotbarSlots;
        m_selectedIndex = i;
    }

    int Inventory::GetSelectedItemId() const {
        const auto& s = m_hotbar[m_selectedIndex];
        return (int)s.itemId;
    }

} // namespace Framework
