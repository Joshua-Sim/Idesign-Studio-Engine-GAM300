///////////////////////////////////////////////////////////////////////////////
///
///@file AnimationSystem.cpp
/// 
/// @brief An Animation system which uses dt to loop through a map containg the files of the images.
///  
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include "AnimationSystem.h"
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 
#include "EngineState.h"



extern Framework::Coordinator ecsInterface;



namespace Framework {




    // Constructor
    AnimationSystem::AnimationSystem()
    {

    }

    // Destructor
    AnimationSystem::~AnimationSystem() {

    }

    // Initialize the system
    void AnimationSystem::Initialize() {
        std::cout << "HEREEEEEEEEEEEEEEEEEEEEEEEEEEE";
        ecsInterface.RegisterComponent<AnimationComponent>();
        Signature signature;
        signature.set(ecsInterface.GetComponentType<AnimationComponent>());
        ecsInterface.SetSystemSignature<AnimationSystem>(signature);
        std::cout << "Signature for Animation system is: " << signature << std::endl;
        animations["McIdle"] = { "McIdle1", "McIdle2", "McIdle3", "McIdle4", "McIdle5", "McIdle6", "McIdle7", "McIdle8",
    "McIdle9", "McIdle10", "McIdle11", "McIdle12", "McIdle13", "McIdle14", "McIdle15" };
        animations["PoisonIdle"] = { "PoisonIdle1", "PoisonIdle2", "PoisonIdle3", "PoisonIdle4", "PoisonIdle5",
            "PoisonIdle6", "PoisonIdle7", "PoisonIdle8", "PoisonIdle9", "PoisonIdle10", "PoisonIdle11", "PoisonIdle12" };
    }

    // Update the system
    void AnimationSystem::Update(float deltaTime) {
        for (auto const& entityId : mEntities) {
            RenderComponent& render = ecsInterface.GetComponent<RenderComponent>(entityId);
            AnimationComponent& animation = ecsInterface.GetComponent<AnimationComponent>(entityId);
            animation.totalFrameTime += deltaTime;
            if (animation.totalFrameTime >= animation.interval) {
                animation.totalFrameTime = 0;
                
                if (animation.animationIndex < animations[animation.key].size()) {

                   render.textureID = animations[animation.key][animation.animationIndex];
                    animation.animationIndex++;
                }
                else {
                    animation.animationIndex = 0;
                    render.textureID = animations[animation.key][animation.animationIndex];
                    animation.animationIndex++;
                }
            }
        } 
        
    }

    // Get the name of the system
    std::string AnimationSystem::GetName() {
        return "Animation System";
    }


}