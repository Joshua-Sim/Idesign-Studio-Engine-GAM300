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
            "Assets/Textures/Loading11.png", // Loading.
            "Assets/Textures/Loading12.png", // Loading..
            "Assets/Textures/Loading13.png",  // Loading...
            "Assets/Textures/Loading4.png",
            "Assets/Textures/Loading5.png",
            "Assets/Textures/Loading6.png",
            "Assets/Textures/Loading7.png",
            "Assets/Textures/Loading8.png",
            "Assets/Textures/Loading9.png",
            "Assets/Textures/Loading10.png"
        };
    };
}