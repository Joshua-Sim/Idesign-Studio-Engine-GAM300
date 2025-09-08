#pragma once
///////////////////////////////////////////////////////////////////////////////
///
///@file AnimationSystem.h
/// 
/// @brief The creation of the Animation class and the declaration of its functions and members
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once //Makes sure this header is only included once

#include "pch.h"
#include "System.h"
#include "Vector2D.h"
#include "InputHandler.h"
#include "ComponentList.h"



namespace Framework {

    class AnimationSystem : public ISystem {
    public:
        AnimationSystem();
        ~AnimationSystem();

        void Initialize();
        void Update(float deltaTime) override;
        std::string GetName() override;



    private:

        std::unordered_map<std::string, std::vector<std::string>> animations;

    };




}