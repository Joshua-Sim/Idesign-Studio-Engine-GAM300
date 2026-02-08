#include "pch.h"
#include "Inventory.h"

namespace Framework {

    Inventory& Inventory::Instance()
    {
        static Inventory g;
        return g;
    }

    // -------------------------
    // Hotbar basic operations
    // -------------------------

    int Inventory::AddFirstEmpty(int itemId, GLuint iconTex, int count)
    {
        // Hotbar only, within unlocked slots
        const int hotSlots = GetHotbarSlotCount();
        for (int i = 0; i < hotSlots; ++i)
        {
            if (m_hotbar[i].itemId == -1)
            {
                m_hotbar[i] = { itemId, iconTex, count };
                return i;
            }
        }
        return -1;
    }

    void Inventory::SetHotbarSlot(int idx, int itemId, GLuint iconTex, int count)
    {
        if (idx < 0 || idx >= GetHotbarSlotCount()) return;
        m_hotbar[idx] = { itemId, iconTex, count };
    }

    void Inventory::ClearHotbarSlot(int idx)
    {
        if (idx < 0 || idx >= GetHotbarSlotCount()) return;
        m_hotbar[idx] = { -1, 0, 0 };
    }

    void Inventory::SetSelectedIndex(int idx)
    {
        int maxSlots = GetHotbarSlotCount();
        if (maxSlots <= 0) maxSlots = 1;

        if (idx < 0) idx = 0;
        if (idx >= maxSlots) idx = maxSlots - 1;
        m_selectedIndex = idx;
    }

    void Inventory::ChangeSelected(int delta)
    {
        delta = -delta;

        int maxSlots = GetHotbarSlotCount();
        if (maxSlots <= 0) maxSlots = 1;

        int i = (m_selectedIndex + delta) % maxSlots;
        if (i < 0) i += maxSlots;
        m_selectedIndex = i;
    }

    int Inventory::GetSelectedItemId() const
    {
        const int maxSlots = GetHotbarSlotCount();
        if (m_selectedIndex < 0 || m_selectedIndex >= maxSlots) return -1;
        return m_hotbar[m_selectedIndex].itemId;
    }

    // -------------------------
    // Hotbar upgrade
    // -------------------------

    void Inventory::SetHotbarLevel(int level)
    {
        if (level < 0) level = 0;
        mHotbarLevel = level;

        int maxSlots = GetHotbarSlotCount();
        if (m_selectedIndex >= maxSlots) m_selectedIndex = maxSlots - 1;
        if (m_selectedIndex < 0) m_selectedIndex = 0;
    }

    int Inventory::GetHotbarLevel() const
    {
        return mHotbarLevel;
    }

    int Inventory::GetHotbarSlotCount() const
    {
        // base 5, +2 per level
        int slots = kHotbarSlots + (mHotbarLevel * 2);

        if (slots < kHotbarSlots) slots = kHotbarSlots;
        if (slots > MaxHotbarSlots) slots = MaxHotbarSlots;
        return slots;
    }

    // -------------------------
    // Backpack
    // -------------------------

    void Inventory::SetBackpackLevel(int level)
    {
        if (level < 1) level = 1;
        if (level > 3) level = 3;
        mBackpackLevel = level;
    }

    int Inventory::GetBackpackLevel() const
    {
        return mBackpackLevel;
    }

    int Inventory::GetBackpackRowCount() const
    {
        return mBackpackLevel; // 0..3
    }

    int Inventory::GetBackpackSlotCount() const
    {
        return GetBackpackRowCount() * kBackpackCols; // 0..30
    }

    bool Inventory::IsBackpackOpen() const
    {
        return mBackpackOpen;
    }

    void Inventory::ToggleBackpack()
    {
        mBackpackOpen = !mBackpackOpen;
        if (!mBackpackOpen) CancelDrag();
    }

    void Inventory::SetBackpackOpen(bool open)
    {
        mBackpackOpen = open;
        if (!mBackpackOpen) CancelDrag();
    }

    const InvSlot& Inventory::GetBackpack(int idx) const
    {
        static InvSlot empty{ -1, 0, 0 };
        if (idx < 0 || idx >= kBackpackMaxSlots) return empty;
        return mBackpack[idx];
    }

    InvSlot& Inventory::GetBackpack(int idx)
    {
        return mBackpack[idx];
    }

    // -------------------------
    // Slot active/unlocked check
    // -------------------------

    bool Inventory::IsSlotActive(SlotRef r) const
    {
        if (!r.IsValid()) return false;

        if (r.kind == SlotKind::Hotbar)
            return r.idx >= 0 && r.idx < GetHotbarSlotCount();

        if (r.kind == SlotKind::Backpack)
            return r.idx >= 0 && r.idx < GetBackpackSlotCount();

        return false;
    }

    // -------------------------
    // Pickup: Backpack first
    // -------------------------

    bool Inventory::TryAddItem_BackpackFirst(int itemId, GLuint iconTex, int count)
    {
        int hotSlots = GetHotbarSlotCount();
        for (int i = 0; i < hotSlots; ++i)
        {
            if (m_hotbar[i].itemId == -1)
            {
                m_hotbar[i] = { itemId, iconTex, count };
                return true;
            }
        }

        int backSlots = GetBackpackSlotCount();
        for (int i = 0; i < backSlots; ++i)
        {
            if (mBackpack[i].itemId == -1)
            {
                mBackpack[i] = { itemId, iconTex, count };
                return true;
            }
        }

        return false;
    }

    bool Inventory::IsInventoryFull() const
    {
        // Backpack full?
        int backSlots = GetBackpackSlotCount();
        bool backFull = true;
        for (int i = 0; i < backSlots; ++i)
        {
            if (mBackpack[i].itemId == -1) { backFull = false; break; }
        }

        // Hotbar full?
        int hotSlots = GetHotbarSlotCount();
        bool hotFull = true;
        for (int i = 0; i < hotSlots; ++i)
        {
            if (m_hotbar[i].itemId == -1) { hotFull = false; break; }
        }

        return backFull && hotFull;
    }

    // -------------------------
    // Drag & drop
    // -------------------------

    bool Inventory::IsDragging() const { return mDragging; }
    SlotRef Inventory::GetDragFrom() const { return mDragFrom; }

    InvSlot Inventory::GetDraggedItemCopy() const
    {
        if (!mDragging || !mDragFrom.IsValid()) return InvSlot{ -1, 0, 0 };

        if (mDragFrom.kind == SlotKind::Hotbar)   return Get(mDragFrom.idx);
        if (mDragFrom.kind == SlotKind::Backpack) return GetBackpack(mDragFrom.idx);
        return InvSlot{ -1, 0, 0 };
    }

    void Inventory::BeginDrag(SlotRef from)
    {
        if (!IsSlotActive(from)) return;

        const InvSlot* s = nullptr;
        if (from.kind == SlotKind::Hotbar)   s = &Get(from.idx);
        if (from.kind == SlotKind::Backpack) s = &GetBackpack(from.idx);

        if (!s || s->itemId < 0) return;

        mDragging = true;
        mDragFrom = from;
    }

    void Inventory::CancelDrag()
    {
        mDragging = false;
        mDragFrom = {};
    }

    void Inventory::DropOn(SlotRef to)
    {
        if (!mDragging || !mDragFrom.IsValid())
            return;

        if (!IsSlotActive(to))
        {
            CancelDrag();
            return;
        }

        if (to.kind == mDragFrom.kind && to.idx == mDragFrom.idx)
        {
            CancelDrag();
            return;
        }

        auto getSlotPtr = [&](SlotRef r) -> InvSlot*
            {
                if (r.kind == SlotKind::Hotbar)   return &Get(r.idx);
                if (r.kind == SlotKind::Backpack) return &GetBackpack(r.idx);
                return nullptr;
            };

        InvSlot* src = getSlotPtr(mDragFrom);
        InvSlot* dst = getSlotPtr(to);
        if (!src || !dst) { CancelDrag(); return; }

        if (dst->itemId < 0)
        {
            *dst = *src;
            *src = InvSlot{ -1, 0, 0 };
        }
        else
        {
            std::swap(*src, *dst);
        }

        CancelDrag();
    }

    // ---------
    // Recipe crafting helper from back back / inventory
    // ---------

   // Helper: Recursively expand a recipe to find base ingredients needed
    struct RecipeRequirements {
        std::unordered_map<int, int> baseIngredients; // itemId -> count
        bool valid = true; // false if circular dependency
    };

    static RecipeRequirements ExpandRecipeToBase(int itemId, int quantity = 1,
        std::unordered_set<int> visited = {})
    {
        RecipeRequirements result;

        // Circular dependency check
        if (visited.count(itemId)) {
            std::cerr << "[Recipe] Circular dependency detected for item " << itemId << "\n";
            result.valid = false;
            return result;
        }
        visited.insert(itemId);

        // Find recipe that produces this item
        const PotionRecipe* recipe = GetRecipeForResult(itemId);

        // If no recipe exists, it's a BASE ingredient (can't be crafted)
        if (!recipe) {
            result.baseIngredients[itemId] += quantity;
            return result;
        }

        // Recursively expand each ingredient in the recipe
        for (int ingredientId : recipe->ingredientIDs) {
            auto subReq = ExpandRecipeToBase(ingredientId, quantity, visited);

            if (!subReq.valid) {
                result.valid = false;
                return result;
            }

            // Merge base ingredients
            for (const auto& [baseId, count] : subReq.baseIngredients) {
                result.baseIngredients[baseId] += count;
            }
        }

        return result;
    }

    // Check if we can craft a recipe (with chain awareness, ignoring tools)
    static bool CanCraftRecipe(const PotionRecipe& recipe)
    {
        auto& inv = Inventory::Instance();

        // Expand recipe to find what base ingredients it REALLY needs
        RecipeRequirements req = ExpandRecipeToBase(recipe.resultItemID);

        if (!req.valid) return false;

        // Build inventory availability map
        std::unordered_map<int, int> available;

        // Scan hotbar
        for (int i = 0; i < inv.GetHotbarSlotCount(); ++i) {
            const auto& slot = inv.Get(i);
            if (slot.itemId >= 0) {
                available[slot.itemId] += (slot.count > 0 ? slot.count : 1);
            }
        }

        // Scan backpack
        for (int i = 0; i < inv.GetBackpackSlotCount(); ++i) {
            const auto& slot = inv.GetBackpack(i);
            if (slot.itemId >= 0) {
                available[slot.itemId] += (slot.count > 0 ? slot.count : 1);
            }
        }

        // Check if we have all BASE ingredients
        for (const auto& [itemId, needed] : req.baseIngredients) {
            if (available[itemId] < needed) {
                return false; // Missing base ingredient
            }
        }

        return true;
    }

    // Get all craftable potion names (with chain awareness)
    std::vector<std::string> GetCraftablePotionNames()
    {
        const auto& recipes = GetGlobalRecipes();
        std::vector<std::string> craftable;

        for (const auto& recipe : recipes) {
            // Only include POTIONS (not intermediate ingredients)
            if (CanCraftRecipe(recipe) &&
                GetItemType(recipe.resultItemID) == ItemType::POTION) {
                craftable.push_back(kItemNames[recipe.resultItemID]);
            }
        }

        return craftable;
    }

} // namespace Framework
