#pragma once
///////////////////////////////////////////////////////////////////////////////
///
///@file EnemyManager.h
/// 
/// @brief The creation of the enemy class and the declaration of its functions and members
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

    class EnemyManager : public ISystem {
    public:
        EnemyManager();
        ~EnemyManager();

        void Initialize();
        void Update(float deltaTime) override;
        std::string GetName() override;



        void moveEnenmyToPlayer(float deltaTime);
    private:



    };
}
extern std::vector<Framework::Entity> entitiesToDestroy;