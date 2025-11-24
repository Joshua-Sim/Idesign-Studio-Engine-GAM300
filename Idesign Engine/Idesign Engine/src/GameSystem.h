#pragma once
#include "Coordinator.h"
#include <string>

namespace Framework {

    // Inherit from ISystem (the interface you shared)
    class GameSystem : public ISystem {
    public:
        static bool bookOpen;
        static bool bookOpened;
        // 1. FIX: Rename Init -> Initialize to match the base class
        void Initialize() override;

        // 2. FIX: Implement Update to satisfy pure virtual = 0
        void Update(float timeslice) override;

        // 3. FIX: Implement GetName to satisfy pure virtual = 0
        std::string GetName() override { return "GameSystem"; }


        static int score;

        static void SetScore(int value);
        static void AddScore(int delta);
        static int  GetScore();

    };
}
