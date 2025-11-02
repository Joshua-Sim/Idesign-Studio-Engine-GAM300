///////////////////////////////////////////////////////////////////////////////
///
/// @file AudioSystem.cpp
/// @brief System implementation for managing 3D audio attached to entities
/// 
/// @Authors: Belinda Chua 
/// @Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "AudioSystem.h"
#include "Coordinator.h"
#include "TransformProperties.h"
#include "ImguiManager.h"
#include <iostream>
#include <cmath>

// IMPORTANT: Use the global ecsInterface like ScriptingSystem does
extern Framework::Coordinator ecsInterface;

namespace Framework {

    AudioSystem::AudioSystem()
        : listenerPosition(0.0f),
        listenerForward(0.0f, 0.0f, -1.0f),
        listenerUp(0.0f, 1.0f, 0.0f),
        listenerVelocity(0.0f),
        lastListenerPosition(0.0f) {
    }

    AudioSystem::~AudioSystem() {
        // Clean up all active audio instances
        for (auto& pair : activeInstances) {
            if (pair.second) {
                pair.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                pair.second->release();
            }
        }
        activeInstances.clear();
    }

    void AudioSystem::Initialize() {
        std::cout << "[AudioSystem] Initializing..." << std::endl;
        ImguiManager::Log("Initializing...", "AudioSystem");

        // 1) AudioComponent - should only be registered by AudioSystem
        std::cout << "[AudioSystem] Registering AudioComponent..." << std::endl;
        ImguiManager::Log("Registering AudioComponent...", "AudioSystem");

        ecsInterface.RegisterComponent<AudioComponent>();

        std::cout << "[AudioSystem] AudioComponent registered" << std::endl;
        ImguiManager::LogSuccess("AudioComponent registered", "AudioSystem");

        // 2) TransformComponent - might already be registered
        std::cout << "[AudioSystem] Checking if TransformComponent is already registered..." << std::endl;
        try {
            // Try to get the component type - if this succeeds, it's already registered
            (void)ecsInterface.GetComponentType<TransformComponent>();
            std::cout << "[AudioSystem] TransformComponent already registered by another system (OK)" << std::endl;
        }
        catch (...) {
            // Not registered yet, we need to register it
            std::cout << "[AudioSystem] TransformComponent not registered, registering now..." << std::endl;
            ecsInterface.RegisterComponent<TransformComponent>();
            std::cout << "[AudioSystem] TransformComponent registered" << std::endl;
        }

        // 3) Set signature (now safe - both components guaranteed to be registered)
        Signature sig;
        sig.set(ecsInterface.GetComponentType<AudioComponent>(), true);
        sig.set(ecsInterface.GetComponentType<TransformComponent>(), true);
        ecsInterface.SetSystemSignature<AudioSystem>(sig);

        std::stringstream ss;
        ss << sig;
        std::cout << "[AudioSystem] Signature set: " << sig << std::endl;
        ImguiManager::Log("Signature set: " + ss.str(), "AudioSystem");

        // 3) Subscribe to messages for entity lifecycle management
        MessageBus::Get().Subscribe<EntityDestroyedMessage>(
            [this](const EntityDestroyedMessage& msg) { OnEntityDestroyed(msg); });

        MessageBus::Get().Subscribe<PlaySoundMessage>(
            [this](const PlaySoundMessage& msg) { OnPlaySound(msg); });

        std::cout << "[AudioSystem] Initialized with 3D audio support" << std::endl;
        ImguiManager::LogSuccess("Initialized with 3D audio support", "AudioSystem");
    }

    void AudioSystem::Update(float deltaTime) {
        // Update listener velocity for Doppler effect
        if (deltaTime > 0.0f) {
            listenerVelocity = (listenerPosition - lastListenerPosition) / deltaTime;
            lastListenerPosition = listenerPosition;
        }

        // Update FMOD listener attributes
        FMOD_3D_ATTRIBUTES listenerAttribs;
        listenerAttribs.position = { listenerPosition.x, listenerPosition.y, listenerPosition.z };
        listenerAttribs.velocity = { listenerVelocity.x, listenerVelocity.y, listenerVelocity.z };
        listenerAttribs.forward = { listenerForward.x, listenerForward.y, listenerForward.z };
        listenerAttribs.up = { listenerUp.x, listenerUp.y, listenerUp.z };

        // Set the listener attributes in FMOD
        FMOD::Studio::System* studioSystem = GlobalAudio.GetStudioSystem();
        if (studioSystem) {
            studioSystem->setListenerAttributes(0, &listenerAttribs);
        }

        // Update all entity audio positions (using mEntities like ScriptingSystem)
        for (Entity entity : mEntities) {
            // Check if entity still has required components
            if (!ecsInterface.HasComponent<AudioComponent>(entity)) continue;
            if (!ecsInterface.HasComponent<TransformComponent>(entity)) continue;

            auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);

            if (audioComp.is3D && audioComp.isPlaying && audioComp.eventInstance) {
                // Calculate velocity for Doppler effect
                glm::vec3 velocity = (transform.position - audioComp.lastPosition) / deltaTime;

                // Update 3D attributes
                Update3DAttributes(entity, transform.position, velocity);

                // Store last position
                audioComp.lastPosition = transform.position;

                // Optional: Calculate distance for debugging
                float distance = glm::length(transform.position - listenerPosition);

                // You could also manually control volume based on distance if needed
                // float attenuation = CalculateVolumeAttenuation(distance, audioComp.minDistance, audioComp.maxDistance);
                // audioComp.eventInstance->setVolume(audioComp.volume * attenuation);
            }
        }

        // Update FMOD
        GlobalAudio.Update();
    }

    void AudioSystem::PlayEntitySound(Entity entity) {
        // Check if entity has required components
        if (!ecsInterface.HasComponent<AudioComponent>(entity)) {
            std::cerr << "[AudioSystem] Entity " << entity << " missing AudioComponent!" << std::endl;
            return;
        }

        auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);

        // Stop existing instance if any
        if (audioComp.eventInstance) {
            audioComp.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            audioComp.eventInstance->release();
            audioComp.eventInstance = nullptr;
        }

        // Load and create new instance
        FMOD::Studio::EventDescription* eventDesc = GlobalAudio.UE_LoadEvent(audioComp.eventName);
        if (!eventDesc) {
            std::cerr << "[AudioSystem] Failed to load event: " << audioComp.eventName << std::endl;
            return;
        }

        FMOD_RESULT result = eventDesc->createInstance(&audioComp.eventInstance);
        if (result != FMOD_OK) {
            std::cerr << "[AudioSystem] Failed to create instance for: " << audioComp.eventName << std::endl;
            return;
        }

        // Set initial properties
        audioComp.eventInstance->setVolume(audioComp.volume);

        // Set 3D attributes if this is a 3D sound
        if (audioComp.is3D && ecsInterface.HasComponent<TransformComponent>(entity)) {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            Update3DAttributes(entity, transform.position, glm::vec3(0.0f));

            // Set min/max distance
            audioComp.eventInstance->setProperty(
                FMOD_STUDIO_EVENT_PROPERTY_MINIMUM_DISTANCE, audioComp.minDistance);
            audioComp.eventInstance->setProperty(
                FMOD_STUDIO_EVENT_PROPERTY_MAXIMUM_DISTANCE, audioComp.maxDistance);
        }

        // Start playback
        audioComp.eventInstance->start();
        audioComp.isPlaying = true;
        activeInstances[entity] = audioComp.eventInstance;

        std::cout << "[AudioSystem] Playing sound '" << audioComp.eventName
            << "' for entity " << entity << std::endl;
    }

    void AudioSystem::StopEntitySound(Entity entity) {
        if (!ecsInterface.HasComponent<AudioComponent>(entity)) return;

        auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);

        if (audioComp.eventInstance) {
            audioComp.eventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
            audioComp.isPlaying = false;
            activeInstances.erase(entity);
        }
    }

    void AudioSystem::SetEntityVolume(Entity entity, float volume) {
        if (!ecsInterface.HasComponent<AudioComponent>(entity)) return;

        auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);
        audioComp.volume = glm::clamp(volume, 0.0f, 1.0f);

        if (audioComp.eventInstance) {
            audioComp.eventInstance->setVolume(audioComp.volume);
        }
    }

    void AudioSystem::SetListenerPosition(const glm::vec3& position,
        const glm::vec3& forward,
        const glm::vec3& up) {
        listenerPosition = position;
        listenerForward = glm::normalize(forward);
        listenerUp = glm::normalize(up);
    }

    void AudioSystem::OnEntityCreated(Entity entity) {
        // Check if entity has audio component
        if (ecsInterface.HasComponent<AudioComponent>(entity)) {
            auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);

            // Auto-play if configured
            if (audioComp.playOnStart && !audioComp.eventName.empty()) {
                PlayEntitySound(entity);
            }
        }
    }

    void AudioSystem::OnEntityDestroyed(const EntityDestroyedMessage& msg) {
        // Clean up audio for destroyed entity
        auto it = activeInstances.find(msg.entity);
        if (it != activeInstances.end()) {
            if (it->second) {
                it->second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                it->second->release();
            }
            activeInstances.erase(it);
        }
    }

    void AudioSystem::OnPlaySound(const PlaySoundMessage& msg) {
        if (msg.is3D) {
            // For 3D sounds without an entity, create a temporary event at position
            FMOD::Studio::EventDescription* eventDesc = GlobalAudio.UE_LoadEvent(msg.soundId);
            if (eventDesc) {
                FMOD::Studio::EventInstance* instance = nullptr;
                if (eventDesc->createInstance(&instance) == FMOD_OK) {
                    // Set 3D attributes
                    FMOD_3D_ATTRIBUTES attributes = {};
                    attributes.position = { msg.position.x, msg.position.y, msg.position.z };
                    attributes.velocity = { 0.0f, 0.0f, 0.0f };
                    attributes.forward = { 0.0f, 0.0f, 1.0f };
                    attributes.up = { 0.0f, 1.0f, 0.0f };

                    instance->set3DAttributes(&attributes);
                    instance->setVolume(msg.volume);
                    instance->start();

                    // Note: This instance won't be tracked, so it will play once and stop
                    // For persistent sounds, attach to an entity instead
                }
            }
        }
        else {
            // Play as 2D sound
            GlobalAudio.UE_PlaySound(msg.soundId);
        }
    }

    void AudioSystem::Update3DAttributes(Entity entity, const glm::vec3& position, const glm::vec3& velocity) {
        if (!ecsInterface.HasComponent<AudioComponent>(entity)) return;

        auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);

        if (audioComp.eventInstance) {
            FMOD_3D_ATTRIBUTES attributes = {};
            attributes.position = { position.x, position.y, position.z };
            attributes.velocity = { velocity.x, velocity.y, velocity.z };
            attributes.forward = { 0.0f, 0.0f, 1.0f }; // Default forward
            attributes.up = { 0.0f, 1.0f, 0.0f };      // Default up

            audioComp.eventInstance->set3DAttributes(&attributes);
        }
    }

    float AudioSystem::CalculateVolumeAttenuation(float distance, float minDist, float maxDist) {
        if (distance <= minDist) {
            return 1.0f; // Full volume
        }
        else if (distance >= maxDist) {
            return 0.0f; // Silent
        }
        else {
            // Linear falloff (you can change this to logarithmic or custom curve)
            float range = maxDist - minDist;
            float distFromMin = distance - minDist;
            return 1.0f - (distFromMin / range);
        }
    }

} // namespace Framework