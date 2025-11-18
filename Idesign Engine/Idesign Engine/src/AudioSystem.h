///////////////////////////////////////////////////////////////////////////////
///
/// @file AudioSystem.h
/// @brief System for managing 3D audio attached to entities
/// 
/// @Authors: Belinda Chua Jia Xuan 
/// @Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef AUDIO_SYSTEM_H
#define AUDIO_SYSTEM_H

#include "System.h"
#include "Audio.h"
#include "AudioComponent.h"
#include "MessageSystem.h"
#include <glm.hpp>
#include <unordered_map>

namespace Framework {

    // Forward declarations
    struct TransformComponent;

    class AudioSystem : public ISystem {
    public:
        AudioSystem();
        ~AudioSystem();

        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override { return "AudioSystem"; }

        // Audio control methods
        void PlayEntitySound(Entity entity);
        void StopEntitySound(Entity entity);
        void SetEntityVolume(Entity entity, float volume);
        void SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

        // Message handlers
        void OnEntityCreated(Entity entity);
        void OnEntityDestroyed(const EntityDestroyedMessage& msg);
        void OnPlaySound(const PlaySoundMessage& msg);

        void OnEnterPlayMode();
        void OnExitPlayMode();


    private:
        // Update 3D attributes for a specific entity's sound
        void Update3DAttributes(Entity entity, const glm::vec3& position, const glm::vec3& velocity);

        // Calculate distance-based volume attenuation
        float CalculateVolumeAttenuation(float distance, float minDist, float maxDist);

        // Listener properties (typically the player/camera)
        glm::vec3 listenerPosition;
        glm::vec3 listenerForward;
        glm::vec3 listenerUp;
        glm::vec3 listenerVelocity;
        glm::vec3 lastListenerPosition;

        // Cache of active audio instances
        std::unordered_map<Entity, FMOD::Studio::EventInstance*> activeInstances;
    };

}

#endif // AUDIO_SYSTEM_H