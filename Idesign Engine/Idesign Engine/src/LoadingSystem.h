#pragma once
#include "System.h"
#include "Coordinator.h"
#include <vector>
#include <string>

namespace Framework {
    class LoadingSystem : public ISystem {
    public:
        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override { return "LoadingSystem"; }

    private:
        // Event Callbacks
    private:
        bool OnLoadingStart(void* data); 
        bool OnLoadingEnd(void* data);   

        Entity m_LoadingEntity = -1;
        bool m_IsActive = false;
        float m_AnimationTimer = 0.0f;
        int m_CurrentFrame = 0;

        // The 3 images for your loop
        const std::vector<std::string> m_Frames = {
            "Assets/Textures/Loading1.png", // Loading.
            "Assets/Textures/Loading2.png", // Loading..
            "Assets/Textures/Loading3.png"  // Loading...
        };
    };
}