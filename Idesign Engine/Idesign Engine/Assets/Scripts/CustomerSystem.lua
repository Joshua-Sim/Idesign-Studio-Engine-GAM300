-- This is the Customer system script. This system handles all customer system logic such as spawning customer prefab at location(Door), handling shop logics and placement.
-- Customer receive and fulfill shop function will be moved to customer object for isolated responsibilities, for now either here or there.

local CustomerSystem = {}

-- Internal state
local activeCustomer = nil
local hasSpawned = false
local customerPrefabName = "CustomerPrefab" -- must be defined in your prefab registry
local spawnPointName = "CustomerSpawnPoint"
local waitPointName = "CustomerWaitingPoint"

-- Called once on script attach
function CustomerSystem:on_init()
    print("[Lua] CustomerSystem initialized.")

    -- (Optional) Build NavMesh here if needed
    if BuildNavMesh then
        BuildNavMesh()
        print("[Lua] NavMesh built.")
    end
end

-- Called every frame
function CustomerSystem:on_update(dt)
    if not hasSpawned then
        self:spawn_customer()
    end
end

-- Spawns a new customer and moves them to wait point
function CustomerSystem:spawn_customer()
    local spawnPoint = FindEntityByName(spawnPointName)
    if spawnPoint == nil then
        print("[Lua] ERROR: Spawn point not found.")
        return
    end

    -- Spawn the customer prefab at the spawn point
    local customer = SpawnPrefab(customerPrefabName, spawnPoint)
    if customer == nil then
        print("[Lua] ERROR: Failed to spawn customer prefab.")
        return
    end

    -- Store active customer reference (One customer for now, move logic to individual customers after)
    activeCustomer = customer
    hasSpawned = true

    -- Trigger pathfinding to wait point
    PathfindTo(customer, waitPointName)
    print("[Lua] Spawned and pathfinding customer to wait point.")
end

-- Called externally (e.g., from item interaction logic)
function GiveItemToCustomer()
    if activeCustomer then
        print("[Lua] Customer received item. Despawning...")

        -- Despawn entity
        DestroyEntity(activeCustomer)
        activeCustomer = nil

        -- Reset for next customer
        hasSpawned = false
    end
end

return CustomerSystem
