#pragma once
#include "pch.h"
#include <array>
#include <string>
#include <glm.hpp>
#include <FontSystem.h>

namespace Framework {

    // Keep hotbar size in one place
    constexpr int kHotbarSlots = 10;

    // One slot = a single non-stackable item (count can be kept for future proofing)
    struct InvSlot {
        int itemId = -1;   // -1 means empty
        GLuint iconTex = 0;
        int count = 0;    // for now you said non-stackable; keep this for future (>=1)
    };

    class Inventory {
    public:
        static Inventory& Instance();

        const InvSlot& Get(int idx) const { return m_hotbar[idx]; }
        InvSlot& Get(int idx) { return m_hotbar[idx]; }

        // Core API
        int  AddFirstEmpty(int itemId, GLuint iconTex = 0, int count = 1);
        void SetHotbarSlot(int idx, int itemId, GLuint iconTex = 0, int count = 1);
        void ClearHotbarSlot(int idx);

        // --- NEW: selection lives here ---
        int  GetSelectedIndex() const { return m_selectedIndex; }
        void SetSelectedIndex(int idx);
        void ChangeSelected(int delta); 
        int  GetSelectedItemId() const; 

    private:
        Inventory() = default;

        std::array<InvSlot, kHotbarSlots> m_hotbar{};
        int m_selectedIndex = 0;                 // moved from Graphics
    };

} // namespace Framework
