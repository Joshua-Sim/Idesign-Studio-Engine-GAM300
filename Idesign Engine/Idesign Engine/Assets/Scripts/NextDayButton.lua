local M = {}

function M:on_init()
    print("[NextDayButton] on_init called for entity:", self.entity)

    RegisterButtonCallback(self.entity, function(ent)
        print("[NextDayButton] Next Day clicked!")

        -- Leave Upgrade
        PopEngineState()
        PushEngineState("Play")  -- Push Play state
 
        -- -- Push MapSelection state - this will automatically show all entities tagged with "map"
        -- PushEngineState("MapSelection")

        -- Advance day + push proper day transition
        AdvanceDayAndPushTransition()

        print("[NextDayButton] Day advanced and transition pushed")
    end)
end

return M