-- GameStateBackToMenu.lua
-- Attach this to a "Back to Menu" button in your game
-- This will clear all states and return to the main menu

local M = {}

function M:on_init()
    print("[GameStateBackToMenu] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[GameStateBackToMenu] Back to Menu button clicked!")
        
        -- Call the C++ function to reset state to Menu
        GameStateBackToMenu()
        
        print("[GameStateBackToMenu] Returned to main menu")
        
        -- Optional: Reset any game state variables
        -- For example, if you want to reset currency on returning to menu:
        -- SetCurrency(0)
        
        -- Optional: Set UI focused for menu interaction
        SetUIFocused(true)
    end)
end

return M