
#pragma once
#ifndef _SCENE_INTERFACE_H_
#define _SCENE_INTERFACE_H_

#include "System.h"
#include "EngineState.h"

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

	enum class CutscenePhase
	{
		FadeIn,
		FadeOut,
		Done
	};

	static CutscenePhase cutscenePhase = CutscenePhase::FadeIn;
	static float cutsceneStartTime = -1.0f;
	static float fadeOutStartTime = -1.0f;


	// Declare the global instance of EngineState
	extern SceneInterface sceneInterface;
}
#endif // !_SCENE_INTERFACE_H_