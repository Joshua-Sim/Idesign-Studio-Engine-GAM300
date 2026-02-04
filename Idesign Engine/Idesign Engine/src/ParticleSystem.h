#pragma once
#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include "System.h"

namespace Framework
{
    class ParticleSystem : public ISystem
    {
    public:
        void Initialize() override;
        void Update(float dt) override;
        std::string GetName() override { return "ParticleSystem"; }
    };
}

#endif
