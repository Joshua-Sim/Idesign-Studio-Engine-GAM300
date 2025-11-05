///////////////////////////////////////////////////////////////////////////////
///
///@file Dummmy.h
/// 
/// @brief The creation of the physics class and the declaration of its functions and members
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "DummySystem.h"
#include "ComponentList.h"
#include "Coordinator.h"
#include "ImguiManager.h"

extern Framework::Coordinator ecsInterface;
using Framework::ImguiManager;

void DummySystem::Initialize()
{
	std::cout << "Dummy System Initialised" << std::endl;
	ImguiManager::LogSuccess("Dummy System Initialised", "DummySystem");
}

void DummySystem::Update(float timeslice)
{
	(void)timeslice;
	std::cout << "Dummy System Update" << std::endl;
}

std::string DummySystem::GetName()
{
	return "DummySystem";
}

DummySystem::~DummySystem()
{

}
