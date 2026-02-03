#include "pch.h"
#include "Inventory.h"
#include <FontSystem.h>

namespace Framework {

    Inventory::Inventory()
        : m_maxSlots(kHotbarSlots)
        , m_selectedIndex(0)
    {
        m_hotbar.resize(kHotbarSlots);
        for (auto& slot : m_hotbar)
        {
            slot.itemId = -1;
        }
    }

    Inventory& Inventory::Instance() {
        static Inventory g;
        return g;
    }

    int Inventory::AddFirstEmpty(int itemId, GLuint iconTex, int count) 
    {
        for (int i = 0; i < m_maxSlots; ++i) 
        {   // Use m_maxSlots instead
            if (m_hotbar[i].itemId == (unsigned)-1) 
            {
                m_hotbar[i] = { itemId, iconTex, count };
                return i;
            }
        }
        return -1;  // no space
    }

    void Inventory::SetHotbarSlot(int idx, int itemId, GLuint iconTex, int count)
    {
        if (idx < 0 || idx >= m_maxSlots) return;
        m_hotbar[idx] = { itemId, iconTex, count };
    }

    void Inventory::ClearHotbarSlot(int idx)
    {
        if (idx < 0 || idx >= m_maxSlots) return;
        m_hotbar[idx] = { -1, 0, 0 };
    }

    void Inventory::SetSelectedIndex(int idx) 
    {
        if (idx < 0) idx = 0;
        if (idx >= m_maxSlots) idx = m_maxSlots - 1;    // Use m_maxSlots
        m_selectedIndex = idx;
    }

    void Inventory::ChangeSelected(int delta) 
    {
        delta = -delta;
        int i = (m_selectedIndex + delta) % m_maxSlots;  // Use m_maxSlots
        if (i < 0) i += m_maxSlots;
        m_selectedIndex = i;
    }

    int Inventory::GetSelectedItemId() const {
        const auto& s = m_hotbar[m_selectedIndex];
        return (int)s.itemId;
    }

    void Inventory::UpgradeSlots(int additionalSlots) 
    {
        int newSize = m_maxSlots + additionalSlots;
        if (newSize > kMaxHotbarSlots) 
        {
            newSize = kMaxHotbarSlots;
        }

        m_hotbar.resize(newSize);

        // Initialize new slots
        for (int i = m_maxSlots; i < newSize; ++i) 
        {
            m_hotbar[i].itemId = (unsigned)-1;
        }
        m_maxSlots = newSize;
    }

} // namespace Framework
