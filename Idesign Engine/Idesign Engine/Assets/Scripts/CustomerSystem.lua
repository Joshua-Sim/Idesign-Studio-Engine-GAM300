-- This is the Customer system script. This system handles all customer system logic such as spawning customer prefab at location(Door), handling shop logics and placement.
-- Customer receive and fulfill shop function will be moved to customer object for isolated responsibilities, for now either here or there.
-- Customer System Script
local CustomerSystem = {}

-- Configuration
CustomerSystem.customerPrefabGUID = "Customer_Prefab"
CustomerSystem.spawnPointName = "SpawnPoint"
CustomerSystem.waitPointName = "CustomerWaitingPoint"
CustomerSystem.frogDoorbellName = "Frog"
CustomerSystem.spawn_delay = 3.0

-- Internal state
local activeCustomer = nil
local hasSpawned = false
local spawnTimer = 0
local navMeshBuilt = false  -- <-- Track if we've built it

function CustomerSystem:on_init()
    print("[CustomerSystem] Initialized.")
    
    -- Build NavMesh on initialization
    if not navMeshBuilt then
        print("[CustomerSystem] Building NavMesh...")
        local success = BuildNavMesh()
        
        if success then
            print("[CustomerSystem] NavMesh built successfully!")
            navMeshBuilt = true
        else
            print("[CustomerSystem] ERROR: Failed to build NavMesh!")
        end
    end
    
    -- Register global callback for when customer completes order
    CustomerSystemNotifyComplete = function()
        print("[CustomerSystem] Customer completed order.")
        activeCustomer = nil
        hasSpawned = false
        spawnTimer = 0
    end
end

function CustomerSystem:on_update(dt)
    -- Only spawn if NavMesh is ready
    if not navMeshBuilt then
        return
    end
    
    -- Spawn logic with timer
    if not hasSpawned then
        spawnTimer = spawnTimer + dt
        
        if spawnTimer >= self.spawn_delay then
            self:spawn_customer()
            spawnTimer = 0
        end
    end
end

function CustomerSystem:spawn_customer()
    local spawnPoint = FindEntityByName(self.spawnPointName)
    if spawnPoint == nil then
        print("[CustomerSystem] ERROR: Spawn point not found.")
        return
    end
    
    self:play_doorbell()

    local customer = SpawnPrefab(self.customerPrefabGUID, spawnPoint)
    
    if customer == nil then
        print("[CustomerSystem] ERROR: Failed to spawn customer.")
        return
    end
    
    print("[CustomerSystem] Spawned customer entity: " .. customer)
    
    activeCustomer = customer
    hasSpawned = true
    
    -- Pathfinding
    PathfindTo("Customer", self.waitPointName)
    print("[CustomerSystem] Customer pathfinding to wait point.")
end

function CustomerSystem:play_doorbell()
    local frogEntity = FindEntityByName(self.frogDoorbellName)

    if frogEntity == nil then 
        print("[CustomerSystem] WARNING: Frog doorbell entity not found")
        print("[CustomerSystem] Looking for entity named: " .. self.frogDoorbellName)
        return
    end

    PlayEntitySound(frogEntity)
    print("[CustomerSystem] Frog croaked, customer entering shop")
end 

return CustomerSystem