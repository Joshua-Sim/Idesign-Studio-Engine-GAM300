#pragma once
#include "System.h"


class DummySystem : public Framework::ISystem
{
	public:
		void Initialize() override;

		void Update(float timeslice) override;

		std::string GetName() override;

		~DummySystem();
};