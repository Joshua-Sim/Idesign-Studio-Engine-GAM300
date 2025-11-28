
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

		static SceneInterface& Instance()
		{
			static SceneInterface instance;
			return instance;
		}

		bool IsInMenu() const { return mInMenu; }
		void SetMenu(bool value) { mInMenu = value; }

		bool IsPaused() const { return mInPause; }
		void SetPaused(bool value) { mInPause = value; }

		bool IsResumed() const { return mInResume; }
		void SetResumed(bool value) { mInResume = value; }

	private:
		bool mInMenu = false;
		bool mInPause = false;
		bool mInResume = false;
	};
}
#endif // !_SCENE_INTERFACE_H_