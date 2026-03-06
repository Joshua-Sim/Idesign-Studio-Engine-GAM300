
#pragma once
#ifndef _SCENE_INTERFACE_H_
#define _SCENE_INTERFACE_H_

#include "System.h"
#include "EngineState.h"
#include "GameSystem.h"
#include "Upgrade.h"
namespace Framework 
{
	class SceneInterface : public ISystem
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		std::string GetName() override;
	private:
	};

	static float Saturate(float x) { return glm::clamp(x, 0.0f, 1.0f); }

	// Declare the global instance of EngineState
	extern SceneInterface sceneInterface;
}
#endif // !_SCENE_INTERFACE_H_