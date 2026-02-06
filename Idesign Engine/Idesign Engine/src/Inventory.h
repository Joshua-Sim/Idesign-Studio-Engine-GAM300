#pragma once
#include "pch.h"
#include <array>
#include <string>
#include <glm.hpp>
#include <FontSystem.h>

namespace Framework {

    // for backpack size
    constexpr int kBackpackCols = 6;
    constexpr int kBackpackRowsMax = 10;
    constexpr int kBackpackMaxSlots = kBackpackCols * kBackpackRowsMax;

    enum class SlotKind : uint8_t { Hotbar, Backpack, None };

    struct SlotRef
    {
        SlotKind kind = SlotKind::None;
        int idx = -1;
        bool IsValid() const { return kind != SlotKind::None && idx >= 0; }
    };

    // Keep hotbar size in one place
    constexpr int kHotbarSlots = 6;

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

        //for upgrade system
        void SetHotbarLevel(int level);
        int  GetHotbarLevel() const;
        int  GetHotbarSlotCount() const;

        //for backpack
        void SetBackpackLevel(int level);
        int  GetBackpackLevel() const;
        int  GetBackpackSlotCount() const;  // 0..30
        int  GetBackpackRowCount() const;   // 0..3

        bool IsBackpackOpen() const;
        void ToggleBackpack();
        void SetBackpackOpen(bool open);

        // Access backpack slots (storage is always 30; active depends on level)
        const InvSlot& GetBackpack(int idx) const;
        InvSlot& GetBackpack(int idx);

        // Drag & drop state
        bool    IsDragging() const;
        SlotRef GetDragFrom() const;
        InvSlot GetDraggedItemCopy() const;

        void BeginDrag(SlotRef from);
        void CancelDrag();
        void DropOn(SlotRef to); // swaps or moves to empty

        // Utility: tells whether a slotRef is "currently active/unlocked"
        bool IsSlotActive(SlotRef r) const;

        bool TryAddItem_BackpackFirst(int itemId, GLuint iconTex, int count);
        bool IsInventoryFull() const;

    private:
        Inventory() = default;

        std::array<InvSlot, kHotbarSlots>       m_hotbar{};
        std::array<InvSlot, kBackpackMaxSlots>  mBackpack{};

        int  m_selectedIndex = 0;
        int  mHotbarLevel = 1;     // 0 => 4 slots
        int  mBackpackLevel = 0;   // 0..3
        bool mBackpackOpen = false;

        bool   mDragging = false;
        SlotRef mDragFrom{};

    };

} // namespace Framework
