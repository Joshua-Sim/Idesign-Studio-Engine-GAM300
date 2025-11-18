#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

namespace Framework {

    // ---------- Item model ----------
    enum class ItemID : uint16_t {
        Carrot, Onion, Mushroom, Chicken, Salt, Water, Bowl, Soup,
        COUNT
    };

    struct ItemData {
        ItemID     id;
        const char* name;
        bool       stackable = true;
        int        maxStack = 99;
        float      weight = 0.1f;   // per unit
    };

    // Minimal in-header DB for quick start.
    // Later, move to ItemDB.{h,cpp} and load from data files.
    inline const ItemData& GetItemData(ItemID id) {
        static const ItemData kTable[] = {
            { ItemID::Carrot,   "Carrot",   true, 99, 0.10f },
            { ItemID::Onion,    "Onion",    true, 99, 0.10f },
            { ItemID::Mushroom, "Mushroom", true, 99, 0.05f },
            { ItemID::Chicken,  "Chicken",  true, 99, 0.50f },
            { ItemID::Salt,     "Salt",     true, 99, 0.01f },
            { ItemID::Water,    "Water",    true, 99, 1.00f },
            { ItemID::Bowl,     "Bowl",     false, 1, 0.30f },
            { ItemID::Soup,     "Soup",     true, 99, 0.60f },
        };
        size_t idx = static_cast<size_t>(id);
        if (idx >= static_cast<size_t>(ItemID::COUNT)) idx = 0;
        return kTable[idx];
    }

    // ---------- ECS components ----------
    struct InventorySlot { ItemID id; int count; };

    struct InventoryComponent {
        std::vector<InventorySlot> slots;
        int   maxSlots = 16;
        float maxWeight = 10.0f;
        float currWeight = 0.0f;
    };

    struct CollectibleComponent {
        ItemID id;
        int    amount = 1;
        bool   autoPickup = false;
    };

    // Crafting model
    struct RecipeEntry { ItemID id; int count; };
    struct Recipe {
        std::vector<RecipeEntry> inputs;
        RecipeEntry              output;  // id + count
    };

    // Optional: store on player
    struct CraftingComponent {
        std::vector<Recipe> knownRecipes;
    };

    // ---------- Inventory helpers ----------
    namespace Inventory {

        inline int Count(const InventoryComponent& inv, ItemID id) {
            int total = 0;
            for (auto& s : inv.slots) if (s.id == id) total += s.count;
            return total;
        }

        inline bool HasAll(const InventoryComponent& inv, const std::vector<RecipeEntry>& reqs) {
            // quick count map
            std::unordered_map<ItemID, int> have;
            have.reserve(inv.slots.size());
            for (auto& s : inv.slots) have[s.id] += s.count;
            for (auto& r : reqs) {
                auto it = have.find(r.id);
                if (it == have.end() || it->second < r.count) return false;
            }
            return true;
        }

        inline bool Remove(InventoryComponent& inv, ItemID id, int count) {
            for (auto it = inv.slots.begin(); it != inv.slots.end() && count > 0; ) {
                if (it->id == id) {
                    int take = std::min(count, it->count);
                    it->count -= take;
                    count -= take;
                    if (it->count == 0) it = inv.slots.erase(it); else ++it;
                }
                else ++it;
            }
            // weight bookkeeping
            float removedWeight = GetItemData(id).weight * (float)(std::max(0, -count)); // if count < 0 (shouldn’t)
            // More accurate: recompute weight each frame or track decrements on success paths.
            // For now, only adjust on success:
            if (count == 0) inv.currWeight = std::max(0.0f, inv.currWeight - GetItemData(id).weight); // minimal nudge
            return count == 0;
        }

        // Add stackable/non-stackable respecting capacity + weight.
        // Returns true if everything was added.
        inline bool TryAdd(InventoryComponent& inv, ItemID id, int count) {
            if (count <= 0) return true;
            const ItemData& d = GetItemData(id);
            float addWeightTotal = d.weight * count;

            if (inv.currWeight + addWeightTotal > inv.maxWeight) return false;

            if (d.stackable) {
                // fill existing stacks
                for (auto& s : inv.slots) {
                    if (s.id != id) continue;
                    if (s.count >= d.maxStack) continue;
                    int can = std::min(count, d.maxStack - s.count);
                    s.count += can;
                    count -= can;
                    if (count == 0) { inv.currWeight += addWeightTotal; return true; }
                }
                // add new stacks
                while (count > 0) {
                    if ((int)inv.slots.size() >= inv.maxSlots) return false;
                    int put = std::min(count, d.maxStack);
                    inv.slots.push_back({ id, put });
                    count -= put;
                }
                inv.currWeight += addWeightTotal;
                return true;
            }
            else {
                // non-stackable: one per slot
                int needSlots = count;
                if ((int)inv.slots.size() + needSlots > inv.maxSlots) return false;
                for (int i = 0; i < needSlots; ++i) inv.slots.push_back({ id, 1 });
                inv.currWeight += addWeightTotal;
                return true;
            }
        }

        // Simple craft helper: consume inputs and add output if possible.
        inline bool Craft(InventoryComponent& inv, const Recipe& r) {
            if (!HasAll(inv, r.inputs)) return false;
            // remove inputs
            for (auto& in : r.inputs) {
                int toRemove = in.count;
                for (auto it = inv.slots.begin(); it != inv.slots.end() && toRemove > 0; ) {
                    if (it->id == in.id) {
                        int take = std::min(toRemove, it->count);
                        it->count -= take;
                        toRemove -= take;
                        if (it->count == 0) it = inv.slots.erase(it); else ++it;
                    }
                    else ++it;
                }
                if (toRemove > 0) return false; // should not happen due to HasAll
            }
            // recompute weight (cheap & correct)
            inv.currWeight = 0.0f;
            for (auto& s : inv.slots) inv.currWeight += GetItemData(s.id).weight * s.count;

            // add output
            return TryAdd(inv, r.output.id, r.output.count);
        }

    } // namespace Inventory

} // namespace Framework
