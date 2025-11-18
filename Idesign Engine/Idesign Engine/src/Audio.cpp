///////////////////////////////////////////////////////////////////////////////
///
///	@File  : Audio.cpp
/// @Brief : This file implements the Audio system, which manages audio-related
///          tasks using FMOD. Using asset manager to retrieve sound assets to
///          provide functionality to play and control both background music and
///          sound effects.
///          This file is the audio system that is linked to the main engine.
///
///	@Authors : Edwin Leow, Belinda Chua 
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "Audio.h"
#include "AssetManager.h"
#include <filesystem>

namespace Framework 
{
    Audio GlobalAudio;      // Define the global Audio Instance

    // Constructor
    Audio::Audio() 
    {
        //Create Studio System
        FMOD_RESULT res = FMOD::Studio::System::create(&studioSystem);

        if (res != FMOD_OK || !studioSystem) 
        {
            std::cerr << "Failed to create FMOD Studio system: " << res << std::endl;
            return;
        }

        //Initialie studio system. Request core system pointer for DSP access
		res = studioSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_3D_RIGHTHANDED, nullptr);
		if (res != FMOD_OK) 
        {
			std::cerr << "Failed to initialize FMOD Studio system: " << res << std::endl;
			return;
		}

        //Get core system for DSP and low level features
		studioSystem->getCoreSystem(&coreSystem);

        //Configure 3D default settings 
		Configure3DSettings(current3DSettings);

        //Initialize DSP effects for environmental audio 
        InitializeReverb();
    }

    // Destructor
    Audio::~Audio() 
    {
        //Release active event instances 
        for (auto& p : activeEventsById) 
        {
            if (p.second) 
            {
				p.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
				p.second->release();
            }
        }
		activeEventsById.clear();

        for (auto& p : activeEvents) 
        {
            if (p.second) 
            {
                p.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                p.second->release();
            }
        }
		activeEvents.clear();

		//Release event descriptions (no explicit release required for EventDescription in many FMOD versions, but set map empty)
		loadedEvents.clear();

        //Clean up DSP Effects
        if (reverbDSP) 
        {
            reverbDSP->release();
            reverbDSP = nullptr;
        }
        if (lowPassFilter) 
        {
            lowPassFilter->release();
            lowPassFilter = nullptr;
        }

		//Shutdown FMOD Studio system
        if (studioSystem) 
        {
            studioSystem->unloadAll();
            studioSystem->release();
            studioSystem = nullptr;
        }
		coreSystem = nullptr;
    }

    // Initialize the system
    // =============================================================================
// FIX FOR Audio.cpp - Initialize() Function
// The Problem: Banks are loaded but sample data is not loaded, so no audio plays
// The Solution: Load sample data after loading banks
// =============================================================================

    void Audio::Initialize()
    {
        std::cout << "Current working directory: "
            << std::filesystem::current_path() << std::endl;
        std::cout << "Audio (Studio) Initialized with 3D support." << std::endl;

        // Get FMOD version for diagnostics
        unsigned int version = 0;
        if (studioSystem) {
            FMOD::System* coreSystem = nullptr;
            studioSystem->getCoreSystem(&coreSystem);
            if (coreSystem)
            {
                coreSystem->getVersion(&version);
                std::cout << "[Audio] FMOD Engine version: "
                    << ((version >> 16) & 0xFFFF) << "."
                    << ((version >> 8) & 0xFF) << "."
                    << (version & 0xFF) << std::endl;
            }
        }

        // Load the bank files
        FMOD::Studio::Bank* masterBank = nullptr;
        FMOD::Studio::Bank* stringsBank = nullptr;
        FMOD::Studio::Bank* bgmBank = nullptr;
        FMOD::Studio::Bank* sfxBank = nullptr;
        FMOD::Studio::Bank* ambientBank = nullptr;
        FMOD_RESULT result;

        // ============================================================
        // Load Master.strings.bank FIRST (required for event names)
        // ============================================================
        std::cout << "\n[Audio] Loading Master.strings.bank..." << std::endl;
        result = studioSystem->loadBankFile("Master.strings.bank",
            FMOD_STUDIO_LOAD_BANK_NORMAL, &stringsBank);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] ERROR: Failed to load Master.strings.bank" << std::endl;
            std::cerr << "[Audio] Error code: " << result << std::endl;
        }
        else
        {
            std::cout << "[Audio] Master.strings.bank loaded successfully!" << std::endl;
        }

        // ============================================================
        // Load Master.bank (contains metadata/structure)
        // ============================================================
        std::cout << "\n[Audio] Loading Master.bank..." << std::endl;
        result = studioSystem->loadBankFile("Master.bank",
            FMOD_STUDIO_LOAD_BANK_NORMAL, &masterBank);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] ERROR: Failed to load Master.bank" << std::endl;
            std::cerr << "[Audio] Error code: " << result << std::endl;
        }
        else
        {
            std::cout << "[Audio] Master.bank loaded successfully!" << std::endl;
        }

        // ============================================================
        // Load BGM.bank (background music)
        // ============================================================
        std::cout << "\n[Audio] Loading BGM.bank..." << std::endl;
        result = studioSystem->loadBankFile("BGM.bank",
            FMOD_STUDIO_LOAD_BANK_NORMAL, &bgmBank);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] WARNING: Failed to load BGM.bank" << std::endl;
            std::cerr << "[Audio] Error code: " << result << std::endl;
        }
        else
        {
            std::cout << "[Audio] BGM.bank loaded successfully!" << std::endl;
            int eventCount = 0;
            bgmBank->getEventCount(&eventCount);
            std::cout << "[Audio] BGM bank contains " << eventCount << " events" << std::endl;

            // ? CRITICAL FIX: Load sample data for BGM bank
            result = bgmBank->loadSampleData();
            if (result != FMOD_OK)
            {
                std::cerr << "[Audio] WARNING: Failed to load BGM sample data. Error: " << result << std::endl;
            }
            else
            {
                std::cout << "[Audio] ? BGM sample data loaded!" << std::endl;
            }
        }

        // ============================================================
        // Load SFX.bank (sound effects)
        // ============================================================
        std::cout << "\n[Audio] Loading SFX.bank..." << std::endl;
        result = studioSystem->loadBankFile("SFX.bank",
            FMOD_STUDIO_LOAD_BANK_NORMAL, &sfxBank);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] WARNING: Failed to load SFX.bank" << std::endl;
            std::cerr << "[Audio] Error code: " << result << std::endl;
        }
        else
        {
            std::cout << "[Audio] SFX.bank loaded successfully!" << std::endl;
            int eventCount = 0;
            sfxBank->getEventCount(&eventCount);
            std::cout << "[Audio] SFX bank contains " << eventCount << " events" << std::endl;

            // ? CRITICAL FIX: Load sample data for SFX bank
            result = sfxBank->loadSampleData();
            if (result != FMOD_OK)
            {
                std::cerr << "[Audio] WARNING: Failed to load SFX sample data. Error: " << result << std::endl;
            }
            else
            {
                std::cout << "[Audio] ? SFX sample data loaded!" << std::endl;
            }
        }

        // ============================================================
        // Load Ambient.bank (ambient sounds)
        // ============================================================
        std::cout << "\n[Audio] Loading Ambient.bank..." << std::endl;
        result = studioSystem->loadBankFile("Ambient.bank",
            FMOD_STUDIO_LOAD_BANK_NORMAL, &ambientBank);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] WARNING: Failed to load Ambient.bank" << std::endl;
            std::cerr << "[Audio] Error code: " << result << std::endl;
        }
        else
        {
            std::cout << "[Audio] Ambient.bank loaded successfully!" << std::endl;
            int eventCount = 0;
            ambientBank->getEventCount(&eventCount);
            std::cout << "[Audio] Ambient bank contains " << eventCount << " events" << std::endl;

            // ? CRITICAL FIX: Load sample data for Ambient bank
            result = ambientBank->loadSampleData();
            if (result != FMOD_OK)
            {
                std::cerr << "[Audio] WARNING: Failed to load Ambient sample data. Error: " << result << std::endl;
            }
            else
            {
                std::cout << "[Audio] ? Ambient sample data loaded!" << std::endl;
            }
        }

        std::cout << "\n[Audio] ========================================" << std::endl;
        std::cout << "[Audio] All bank files and sample data loaded!" << std::endl;
        std::cout << "[Audio] ========================================\n" << std::endl;

        //Create references to common buses 
        UE_CreateBus("bus:/");          //master bus 
        UE_CreateBus("bus:/BGM");       //BGM bus
        UE_CreateBus("bus:/SFX");       //SFX bus
        UE_CreateBus("bus:/Ambient");   //Ambient bus
    }

    // =============================================================================
    // WHAT THIS FIX DOES:
    // =============================================================================
    // After each bank is loaded, we call bank->loadSampleData() to actually load
    // the audio samples into memory. Without this, FMOD knows about the events
    // but has no actual audio data to play!
    //
    // This is the most common reason for "silent audio" in FMOD Studio.
    // =============================================================================

    // Update the system
    void Audio::Update(float deltaTime) 
    {
        //(void)deltaTime;            // Suprress unused parameter warning

        //Handle fade effects for smooth BGM transitions
        if (isFadingIn || isFadingOut) 
        {
            fadeTimer += deltaTime;
            float progress = fadeTimer / fadeDuration;
            progress = std::min(1.0f, std::max(0.0f, progress));

            if (isFadingIn) 
            {
                float currentVolume = fadeStartVolume + (fadeTargetVolume - fadeStartVolume) * progress;
                if (currentBGMChannel >= 0 && activeEventsById.count(currentBGMChannel)) {
                    activeEventsById[currentBGMChannel]->setVolume(currentVolume * bgmVolume * masterVolume);
                }

                if (progress >= 1.0f) 
                {
                    isFadingIn = false;
                    std::cout << "Fade in complete" << std::endl;
                }
            }
            else if (isFadingOut) 
            {
                float currentVolume = fadeStartVolume * (1.0f - progress);
                if (currentBGMChannel >= 0 && activeEventsById.count(currentBGMChannel)) {
                    activeEventsById[currentBGMChannel]->setVolume(currentVolume * bgmVolume * masterVolume);
                }

                if (progress >= 1.0f) 
                {
                    isFadingOut = false;
                    if (currentBGMChannel >= 0) 
                    {
                        StopChannel(currentBGMChannel);
                    }

                    // Switch to pending BGM if crossfading
                    if (pendingBGMChannel >= 0) 
                    {
                        currentBGMChannel = pendingBGMChannel;
                        pendingBGMChannel = -1;
                        FadeBGMIn(fadeDuration);
                    }
                }
            }
        }
       
        if (studioSystem) 
        {
            studioSystem->update();
        }
    }

    // Get the name of the system
    std::string Audio::GetName() 
    {
        return "Audio (Studio)";
    }

    void Audio::PrintAudioInfo() const
    {
        std::cout << "=== Audio Studio Engine Information ===" << std::endl;
        std::cout << "Loaded Events: " << loadedEvents.size() << std::endl;
        std::cout << "Active Events: " << activeEvents.size() << std::endl;
        std::cout << "Active Events (by id): " << activeEventsById.size() << std::endl;
        std::cout << "======================================" << std::endl;
    }

    Audio::SoundType Audio::ParseSoundType(const std::string& str)
    {
        if (str == "background") return Audio::SoundType::BACKGROUND_MUSIC;
        if (str == "effect") return Audio::SoundType::SOUND_EFFECT;
		return Audio::SoundType::EMPTY;
    }

    // Function to change from String to FMOD_MODE
    FMOD_MODE Audio::UE_GetModeFromString(const std::string& mode)
    {
        if (mode == "loop")         
        {   
            return FMOD_LOOP_NORMAL;    // Looping Mode
        }
        else if (mode == "oneshot")
        {
            return FMOD_LOOP_OFF;       // One-shot playback
        }
        else
        {
            throw std::runtime_error("Invalid sound mode : " + mode);
        }
    }

    //Load an event description using the path stored in AudioProperties.path 
    FMOD::Studio::EventDescription* Audio::UE_LoadEvent(const std::string& name)
    {
        std::cout << "[Audio] UE_LoadEvent called for: " << name << std::endl;

        // Check if already loaded
        auto it = loadedEvents.find(name);
        if (it != loadedEvents.end())
        {
            std::cout << "[Audio] Event already loaded: " << name << std::endl;
            return it->second;
        }

        // Check if the name already has the "event:/" prefix
        std::string eventPath = name;
        if (name.find("event:/") == std::string::npos)
        {
            // Add prefix only if it's not there
            eventPath = "event:/" + name;
            std::cout << "[Audio] Added event prefix: " << eventPath << std::endl;
        }
        else
        {
            std::cout << "[Audio] Using provided path: " << eventPath << std::endl;
        }

        FMOD::Studio::EventDescription* eventDesc = nullptr;
        FMOD_RESULT result = studioSystem->getEvent(eventPath.c_str(), &eventDesc);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] Failed to get event '" << eventPath << "'. Error code: " << result << std::endl;

            // List all available events for debugging
            ListAllEvents();
            return nullptr;
        }

        // Cache the loaded event using the ORIGINAL name passed in
        loadedEvents[name] = eventDesc;
        std::cout << "[Audio] Event loaded and cached: " << name << std::endl;

        return eventDesc;
    }

    void Audio::UE_PlaySound(const std::string& name)
    {
        std::cout << "[Audio] UE_PlaySound called for: " << name << std::endl;

        // First check if event is already loaded
        auto it = loadedEvents.find(name);
        if (it == loadedEvents.end())
        {
            std::cout << "[Audio] Event not in cache, loading: " << name << std::endl;

            // Try to load the event
            FMOD::Studio::EventDescription* eventDesc = UE_LoadEvent(name);
            if (!eventDesc)
            {
                std::cerr << "[Audio] Failed to load event: " << name << std::endl;
                return;
            }
            std::cout << "[Audio] Event loaded successfully: " << name << std::endl;
        }
        else
        {
            std::cout << "[Audio] Event found in cache: " << name << std::endl;
        }

        // Get or retrieve the event description
        FMOD::Studio::EventDescription* eventDesc = nullptr;
        auto loadedIt = loadedEvents.find(name);
        if (loadedIt != loadedEvents.end())
        {
            eventDesc = loadedIt->second;
        }

        if (!eventDesc)
        {
            std::cerr << "[Audio] Event description is null!" << std::endl;
            return;
        }

        // Create an instance
        FMOD::Studio::EventInstance* eventInstance = nullptr;
        FMOD_RESULT result = eventDesc->createInstance(&eventInstance);

        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] Failed to create instance. Error: " << result << std::endl;
            return;
        }

        std::cout << "[Audio] Instance created successfully" << std::endl;

        // Start the instance
        result = eventInstance->start();
        if (result != FMOD_OK)
        {
            std::cerr << "[Audio] Failed to start instance. Error: " << result << std::endl;
            eventInstance->release();
            return;
        }

        std::cout << "[Audio] Sound started playing: " << name << std::endl;

        // Store the instance
        activeEvents[name] = eventInstance;
    }

    //========================== NEW 3D AUDIO FUNCTIONS ==============================
    
    //Play a sound in 3D space with full spatial audio support 
    int Audio::PlaySound3D(
        const std::string& customName,
        const Vector3& position,
        const Vector3& velocity,
        float volume,
        AudioSourceType sourceType)
    {
        // 1. Load the sound if not already loaded
        EventDescription* desc = UE_LoadEvent(customName);
        if (!desc) return -1;

        EventInstance* instance = nullptr;
        FMOD_RESULT res = desc->createInstance(&instance);
        if (res != FMOD_OK || !instance)
        {
            std::cerr << "Failed to create event instance for " << customName
                << " res=" << res << std::endl;
            return -1;
        }

        // 2. Set 3D attributes
        FMOD_3D_ATTRIBUTES attributes = {};
        attributes.position = VectorToFmod(position);
        attributes.velocity = VectorToFmod(velocity);
        attributes.forward = { 0.0f, 0.0f, 1.0f };
        attributes.up = { 0.0f, 1.0f, 0.0f };
        instance->set3DAttributes(&attributes);

        // 3. Resolve metadata
        AssetHandle guid = AssetManager::Instance().FindGuid<AudioMetadata>(customName);
        AudioMetadata* meta = AssetManager::Instance().Get<AudioMetadata>(guid);

        bool looping = meta ? (meta->mode == "loop") : false;
        Audio::SoundType st = ParseSoundType(meta ? meta->soundType : std::string());

        // 4. Set volume
        float finalVolume = volume * masterVolume;
        finalVolume *= (st == BACKGROUND_MUSIC) ? bgmVolume : sfxVolume;
        instance->setVolume(finalVolume);

        // 5. Start playback
        instance->start();

        // 6. Track runtime state in AudioComponent
        int id = GetNextChannelId();
        activeEventsById[id] = instance;
        activeEvents[customName] = instance;

        AudioComponent* comp = AssetManager::Instance().Get<AudioComponent>(guid);
        if (comp) 
        {
            comp->eventInstance = instance;
            comp->channelID = id;
            comp->isPlaying = true;
            comp->lastPosition = { position.x, position.y, position.z };
        }

        // 7. Store 3D info for later updates
        channel3DInfo[id] = 
        {
            position, velocity,
            current3DSettings.minDistance,
            current3DSettings.maxDistance,
            sourceType, looping, volume };

        std::cout << "Playing 3D event: " << customName << " at position ("
            << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
        return id;
    }

    //Set the 3D listener position and orientation
    void Audio::Set3DListener(
        const Vector3& position,
        const Vector3& velocity,
        const Vector3& forward,
        const Vector3& up)
    {
		FMOD_3D_ATTRIBUTES attributes = {};
		attributes.position = VectorToFmod(position);
        attributes.velocity = VectorToFmod(velocity);
        attributes.forward = VectorToFmod(forward);
        attributes.up = VectorToFmod(up);
        if (studioSystem) 
        {
            studioSystem->setListenerAttributes(0, &attributes);
        }
    }

    //Support multiple listeners for split-screen
    void Audio::Set3DListenerMultiple(
        int listenerId,
        const Vector3& position,
        const Vector3& velocity,
        const Vector3& forward,
        const Vector3& up)
    {
        FMOD_3D_ATTRIBUTES attributes = {};
        attributes.position = VectorToFmod(position);
        attributes.velocity = VectorToFmod(velocity);
        attributes.forward = VectorToFmod(forward);
        attributes.up = VectorToFmod(up);

        int numListeners = 0;
        if (studioSystem) 
        {
            studioSystem->getNumListeners(&numListeners);
            if (listenerId >= numListeners) 
            {
                studioSystem->setNumListeners(listenerId + 1);
            }
            studioSystem->setListenerAttributes(listenerId, &attributes);
        }
    }

    //Update 3D attributes for a moving sound 
    void Audio::UpdateChannel3DAttributes(
        int channelId,
        const Vector3& position,
        const Vector3& velocity)
    {
        auto it = activeEventsById.find(channelId);
        if  (it != activeEventsById.end() && it->second) 
        {
            FMOD_3D_ATTRIBUTES attributes = {};
            attributes.position = VectorToFmod(position);
            attributes.velocity = VectorToFmod(velocity);
            attributes.forward = { 0.0f, 0.0f, 1.0f };
            attributes.up = { 0.0f, 1.0f, 0.0f };
            it->second->set3DAttributes(&attributes);

            // Update cached info
            if (channel3DInfo.count(channelId)) 
            {
                channel3DInfo[channelId].position = position;
                channel3DInfo[channelId].velocity = velocity;
            }
        }
    }

    //Update 3D attributes by sound name 
    void Audio::UpdateChannel3DAttributesByName(
        const std::string& customName,
        const Vector3& position,
        const Vector3& velocity)
    {
        auto it = activeEvents.find(customName);
        if (it != activeEvents.end() && it->second) 
        {
            FMOD_3D_ATTRIBUTES attributes = {};
            attributes.position = VectorToFmod(position);
            attributes.velocity = VectorToFmod(velocity);
            attributes.forward = { 0.0f, 0.0f, 1.0f };
            attributes.up = { 0.0f, 1.0f, 0.0f };
            it->second->set3DAttributes(&attributes);
        }
    }

    //Configure global 3D settings 
    void Audio::Configure3DSettings(const Audio3DSettings& settings)
    {
        current3DSettings = settings;
        if (coreSystem) 
        {
            coreSystem->set3DSettings(settings.dopplerScale, settings.distanceFactor, settings.rolloffScale);
        }
    }

    //Set min/max distance for a specific channel
    void Audio::SetChannelMinMaxDistance(int channelId, float minDist, float maxDist)
    {
        //Studio manages attentuation mostly via authoring inside FMOD Studio event
        //We keep cached values so you can still query/update them 
        if (channel3DInfo.count(channelId)) 
        {
            channel3DInfo[channelId].minDistance = minDist;
            channel3DInfo[channelId].maxDistance = maxDist;
        }
    }

    //Set cone for directional sounds 
    void Audio::SetChannel3DConeSettings(
        int channelId,
        float insideConeAngle,
        float outsideConeAngle,
        float outsideVolume,
        const Vector3& orientation)
    {
       //Studio's event emitter has cone settings authored in FMOD Studio 
       //If dynamic cone settings is needed, can adjust the 3D attributes or parameters
        (void)channelId;
        (void)insideConeAngle;
        (void)outsideConeAngle;
        (void)outsideVolume;
        (void)orientation; 
    }

    //Set occlusion for realistic sound blocking 
    void Audio::SetChannelOcclusion(int channelId, float directOcclusion, float reverbOcclusion)
    {
        auto it = activeEventsById.find(channelId);
        if (it != activeEventsById.end() && it->second) 
        {
            // Apply occlusion through a user parameter if authored in FMOD Studio (optional)
            it->second->setParameterByName("Occlusion", directOcclusion);
            it->second->setParameterByName("ReverbOcclusion", reverbOcclusion);

        }
    }

    //Create an audio zone with environmental properties
    void Audio::CreateAudioZone(
        const std::string& zoneName,
        const Vector3& minBounds,
        const Vector3& maxBounds,
        float reverbAmount,
        float lowPassFilterValue)
    {
        audioZones[zoneName] = { minBounds, maxBounds, reverbAmount, lowPassFilterValue };
        std::cout << "Created audio zone: " << zoneName << std::endl;
    }

    //Update listener's current zone for environmental effects
    void Audio::UpdateListenerZone(const Vector3& listenerPos)
    {
        std::string newZone = "";

        // Check which zone the listener is in
        for (const auto& [name, zone] : audioZones) 
        {
            if (listenerPos.x >= zone.minBounds.x && listenerPos.x <= zone.maxBounds.x &&
                listenerPos.y >= zone.minBounds.y && listenerPos.y <= zone.maxBounds.y &&
                listenerPos.z >= zone.minBounds.z && listenerPos.z <= zone.maxBounds.z) 
            {
                newZone = name;
                break;
            }
        }

        // If zone changed, apply new zone settings
        if (newZone != currentZone) 
        {
            currentZone = newZone;

            if (!newZone.empty() && audioZones.count(newZone)) 
            {
                const AudioZone& zone = audioZones[newZone];

                // Apply reverb settings
                if (reverbDSP) 
                {
                    reverbDSP->setBypass(false);
                    reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL,
                        -12.0f + (zone.reverbAmount * 12.0f));
                }

                // Apply low-pass filter
                if (lowPassFilter) 
                {
                    if (zone.lowPassFilter < 1.0f) {
                        lowPassFilter->setBypass(false);
                        lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF,
                            zone.lowPassFilter * 22000.0f);
                    }
                    else 
                    {
                        lowPassFilter->setBypass(true);
                    }
                }

                std::cout << "Entered audio zone: " << newZone << std::endl;
            }
            else 
            {
                // No zone - disable effects
                if (reverbDSP) reverbDSP->setBypass(true);
                if (lowPassFilter) lowPassFilter->setBypass(true);
            }
        }
    }

    //Check if a position is within audible range
    bool Audio::IsPositionAudible(const Vector3& position, float maxAudibleDistance)
    {
        //Query primary listener position via studio system 
        FMOD_3D_ATTRIBUTES attributes = {};

        if (studioSystem) 
        {
            float dx = position.x - attributes.position.x;
            float dy = position.y - attributes.position.y;
            float dz = position.z - attributes.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            return distance <= maxAudibleDistance;
        }
        return false;
    }

    //Initialize reverb and filter DSP effects
    void Audio::InitializeReverb()
    {
        // Create reverb DSP
        if (!coreSystem) return;
        
        coreSystem->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &reverbDSP);
        if (reverbDSP) 
        {
            // Set default reverb properties (room-like)
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, 1.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, 0.02f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, 0.04f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, 5000.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HFDECAYRATIO, 0.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, 100.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, 100.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, 250.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFGAIN, 0.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HIGHCUT, 20000.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYLATEMIX, 0.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, -12.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, 0.0f);

            // Add to master channel group but bypass initially
            FMOD::ChannelGroup* masterGroup;
            coreSystem->getMasterChannelGroup(&masterGroup);
            if (masterGroup) masterGroup->addDSP(0, reverbDSP);
            masterGroup->addDSP(0, reverbDSP);
            reverbDSP->setBypass(true);
        }

        // Create low-pass filter DSP
        coreSystem->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &lowPassFilter);
        if (lowPassFilter) 
        {
            lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f);

            FMOD::ChannelGroup* masterGroup;
            coreSystem->getMasterChannelGroup(&masterGroup);
            masterGroup->addDSP(0, lowPassFilter);
            lowPassFilter->setBypass(true);
        }
    }

    //Apply reverb to a specific channel
    void Audio::ApplyReverbToChannel(int channelId, bool enable)
    {
        // In FMOD Studio, reverb should be routed through buses or sends,
        // not attached directly to event instances.
        (void)channelId;
        (void)enable;
        std::cout << "[FMOD] ApplyReverbToChannel is handled via FMOD Studio routing instead." << std::endl;
    }

    //Check if a channel is currently playing 
    bool Audio::IsChannelPlaying(int channelId)
    {
        auto it = activeEventsById.find(channelId);
        if (it != activeEventsById.end() && it->second) 
        {
            FMOD_STUDIO_PLAYBACK_STATE state;
            it->second->getPlaybackState(&state);
            return state == FMOD_STUDIO_PLAYBACK_PLAYING || state == FMOD_STUDIO_PLAYBACK_SUSTAINING;
        }
        return false;
    }

    //Get the current volume of a channel 
    float Audio::GetChannelVolume(int channelId)
    {
        auto it = activeEventsById.find(channelId);
        if (it != activeEventsById.end() && it->second) 
        {
            float volume = 0.0f;
            it->second->getVolume(&volume);
            return volume;
        }
        return 0.0f;
    }

    //Stop a specific channel by ID
    void Audio::StopChannel(int channelId)
    {
        auto it = activeEventsById.find(channelId);
        if (it != activeEventsById.end() && it->second) 
        {
            it->second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            it->second->release();

            // Clean up the channel from our tracking maps
            activeEventsById.erase(it);

           //Remove name mapping if present 
            for (auto ait = activeEvents.begin(); ait != activeEvents.end(); ++ait) 
            {
                FMOD::Studio::EventInstance* inst = ait->second;
                if (inst == it->second) 
                {
                    activeEvents.erase(ait);
                    break;
                }
            }
            if (channel3DInfo.count(channelId)) channel3DInfo.erase(channelId);
            if (channelId == currentBGMChannel) currentBGMChannel = -1;
        }
    }

    //Stop all currently playing channels 
    void Audio::StopAllChannels()
    {
        // Stop all channels by name
        for (auto& channelEntry : activeEvents) 
        {
            if (channelEntry.second)
            {
                channelEntry.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                channelEntry.second->release();
            }
        }
        activeEvents.clear();

        // Stop all channels by ID
        for (auto& channelEntry : activeEventsById) 
        {
            if (channelEntry.second) 
            {
                channelEntry.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                channelEntry.second->release();
            }
        }
        activeEventsById.clear();
        channel3DInfo.clear();

        // Reset BGM channel
        currentBGMChannel = -1;
        pendingBGMChannel = -1;

        std::cout << "All audio channels stopped" << std::endl;
    }

    //Set master volume level 
    void Audio::SetMasterVolume(float volume)
    {
        masterVolume = std::max(0.0f, std::min(1.0f, volume));
        Bus* masterBus = nullptr;

        // Apply to master channel group
        
        if (studioSystem) 
        {
            studioSystem->getBus("bus:/", &masterBus);
            if (masterBus)
                masterBus->setVolume(masterVolume);
        }
        std::cout << "Master volume set to: " << masterVolume << std::endl;
    }

    //Set BGM Volume level 
    void Audio::SetBGMVolume(float volume)
    {
        bgmVolume = std::max(0.0f, std::min(1.0f, volume));
        Bus* musicBus = nullptr;
        // Apply to BGM channel group
        if (studioSystem) 
        {
            studioSystem->getBus("bus:/Music", &musicBus);
            if (musicBus)
                musicBus->setVolume(bgmVolume);
        }
        std::cout << "BGM volume set to: " << bgmVolume << std::endl;
    }

    //Set SFX volume level 
    void Audio::SetSFXVolume(float volume)
    {
        sfxVolume = std::max(0.0f, std::min(1.0f, volume));
        Bus* sfxBus = nullptr;

        if (studioSystem) 
        {
            studioSystem->getBus("bus:/SFX", &sfxBus);
            if (sfxBus) sfxBus->setVolume(sfxVolume);
            studioSystem->getBus("bus:/Ambient", &sfxBus);
            if (sfxBus) sfxBus->setVolume(sfxVolume);
        }
        std::cout << "SFX volume set to: " << sfxVolume << std::endl;
    }

    //Fade in BGM over specified duration 
    void Audio::FadeBGMIn(float duration)
    {
        if (currentBGMChannel >= 0 && activeEventsById.count(currentBGMChannel)) 
        {
            fadeStartVolume = 0.0f;
            fadeTargetVolume = 1.0f;
            fadeDuration = duration;
            fadeTimer = 0.0f;
            isFadingIn = true;
            isFadingOut = false;

            // Start at zero volume
            activeEventsById[currentBGMChannel]->setVolume(0.0f);

            // Enable low-pass filter for fade-in effect (optional)
            if (lowPassFilter) 
            {
                lowPassFilter->setBypass(false);
                lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 500.0f);
            }
            std::cout << "Starting BGM fade-in over " << duration << " seconds" << std::endl;
        }
    }

    //Fade out BGM over specified duration 
    void Audio::FadeBGMOut(float duration)
    {
        if (currentBGMChannel >= 0 && activeEventsById.count(currentBGMChannel)) 
        {
            float currentVolume;
            activeEventsById[currentBGMChannel]->getVolume(&currentVolume);

            fadeStartVolume = currentVolume;
            fadeTargetVolume = 0.0f;
            fadeDuration = duration;
            fadeTimer = 0.0f;
            isFadingOut = true;
            isFadingIn = false;

            // Enable low-pass filter for fade-out effect (optional)
            if (lowPassFilter) 
            {
                lowPassFilter->setBypass(false);
                lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f);
            }
            std::cout << "Starting BGM fade-out over " << duration << " seconds" << std::endl;
        }
    }

    //Crossfade from current BGM to new BGM
    void Audio::CrossfadeBGM(const std::string& newBGM, float duration)
    {
        // Start new BGM at zero volume
        int newChannelId = PlaySound3D(newBGM, Vector3(0, 0, 0), Vector3(0, 0, 0),
            0.0f, AudioSourceType::STEREO_2D);

        if (newChannelId >= 0) 
        {
            // Store as pending BGM
            pendingBGMChannel = newChannelId;

            // If there's a current BGM, fade it out
            if (currentBGMChannel >= 0) 
            {
                FadeBGMOut(duration);
            }
            else 
            {
                // No current BGM, just fade in the new one
                currentBGMChannel = newChannelId;
                pendingBGMChannel = -1;
                FadeBGMIn(duration);
            }
            std::cout << "Crossfading to BGM: " << newBGM << " over " << duration << " seconds" << std::endl;
        }
    }

    //Set reverb properties for environmental effects 
    void Audio::SetReverbProperties(float decayTime, float wetLevel, float dryLevel)
    {
        if (reverbDSP) 
        {
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, decayTime);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, wetLevel);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, dryLevel);

            std::cout << "Reverb properties updated - Decay: " << decayTime
                << "s, Wet: " << wetLevel << "dB, Dry: " << dryLevel << "dB" << std::endl;
        }
    }

    void Audio::UE_PauseSound(const std::string& customName)
    {
        auto it = activeEvents.find(customName);      // Find the Channel based on the customName
        if (it != activeEvents.end())
        {
            // Studio EventInstance generally supports setPaused
            it->second->setPaused(true);
            std::cout << "Pausing " << customName << std::endl;
        }
        else
        {
            std::cerr << "Error: Channel for " << customName << " not found!" << std::endl;
        }
    }

    void Audio::UE_SetVolume(const std::string& customName, float volume)
    {
        auto it = activeEvents.find(customName);                 // Find the channel associated with the custom sound name

        if (it != activeEvents.end() && it->second != nullptr)
        {
            it->second->setVolume(volume);                       // Set the volume on the channel (0.0f is silent, 1.0f is max volume)
            std::cout << "Volume for " << customName << " set to " << volume << std::endl;
        }
        else
        {
            std::cerr << "Error: Channel for " << customName << " not found!" << std::endl;
        }
    }

    void Audio::UE_SetGroupVolume(FMOD::ChannelGroup* bus, float volume)
    {
        if (bus)
        {
            bus->setVolume(volume);       // Set the volume of the group to the given volume
            std::cout << "Set volume for channel group to " << volume << std::endl;
        }
    }

    void Audio::UE_PauseGroup(FMOD::ChannelGroup* bus)
    {
        if (bus) 
        {
            bool paused = false;
            bus->getPaused(&paused);
            bus->setPaused(!paused);
            std::cout << (paused ? "Resuming bus." : "Pausing bus.") << std::endl;
        }
    }

    void Audio::UE_CreateBus(const std::string& busPath)
    {
        Bus* bus = nullptr;
        if (studioSystem) 
        {
            FMOD_RESULT res = studioSystem->getBus(busPath.c_str(), &bus);
            if (res == FMOD_OK && bus) {
                activeBuses[busPath] = bus;
            }
            else 
            {
                std::cout << "Warning: Bus " << busPath << " not found in FMOD project." << std::endl;
            }
        }
    }

    void Audio::IncrementGroupVol(const std::string& groupName)
    {
        Bus* bus = nullptr;
        if (studioSystem) 
        {
            studioSystem->getBus(groupName.c_str(), &bus);
            if (bus) 
            {
                float cur = 1.0f;
                bus->getVolume(&cur);
                float newV = std::min(1.0f, cur + volChangeAmount);
                bus->setVolume(newV);
                std::cout << "Increased volume of " << groupName << " to " << newV << std::endl;
                return;
            }
        }
        std::cout << "Bus " << groupName << " not found." << std::endl;
    }

    void Audio::DecrementGroupVol(const std::string& groupName)
    {
        Bus* bus = nullptr;
        if (studioSystem) 
        {
            studioSystem->getBus(groupName.c_str(), &bus);
            if (bus) 
            {
                float cur = 1.0f;
                bus->getVolume(&cur);
                float newV = std::max(0.0f, cur - volChangeAmount);
                bus->setVolume(newV);
                std::cout << "Decreased volume of " << groupName << " to " << newV << std::endl;
                return;
            }
        }
        std::cout << "Bus " << groupName << " not found." << std::endl;
    }

    void Audio::PauseAllAudio()
    {
        for (auto& kv : activeEvents) 
        {
            if (kv.second) 
            {
                kv.second->setPaused(true);
            }
        }
    }

    void Audio::ResumeAllAudio()
    {
        for (auto& kv : activeEvents) 
        {
            if (kv.second) 
            {
                kv.second->setPaused(false);
            }
        }
    }

    void Audio::Reset()
    {
        PauseAllAudio();
        for (auto& kv : activeEvents) 
        {
            if (kv.second) 
            {
                kv.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                
            }
        }
        activeEvents.clear();
        std::cout << "Audio system has been reset by stopping events." << std::endl;
    }

    void Audio::ListAllEvents()
    {
        std::cout << "\n[Audio] === Available Events in Banks ===" << std::endl;

        int bankCapacity = 0;
        FMOD_RESULT result;

        // First call: get number of banks
        result = studioSystem->getBankList(nullptr, 0, &bankCapacity);
        if (result != FMOD_OK || bankCapacity == 0)
        {
            std::cout << "[Audio] No banks found or error occurred.\n" << std::endl;
            return;
        }

        // Allocate array
        std::vector<FMOD::Studio::Bank*> banks(bankCapacity);

        // Second call: actually retrieve bank list
        result = studioSystem->getBankList(banks.data(), bankCapacity, nullptr);
        if (result != FMOD_OK)
        {
            std::cout << "[Audio] Failed to retrieve bank list.\n" << std::endl;
            return;
        }

        // Iterate through each bank
        for (auto* bank : banks)
        {
            if (!bank) continue;

            char bankPath[256];
            int pathLen = 0;
            bank->getPath(bankPath, sizeof(bankPath), &pathLen);

            std::cout << "[Audio] Bank: " << bankPath << std::endl;

            int eventCount = 0;
            bank->getEventCount(&eventCount);

            if (eventCount > 0)
            {
                std::vector<FMOD::Studio::EventDescription*> eventList(eventCount);
                bank->getEventList(eventList.data(), eventCount, nullptr);

                for (auto* eventDesc : eventList)
                {
                    if (!eventDesc) continue;

                    char eventPath[256];
                    int pathSize = 0;
                    eventDesc->getPath(eventPath, sizeof(eventPath), &pathSize);

                    std::cout << "[Audio]   - " << eventPath << std::endl;
                }
            }
        }

        std::cout << "[Audio] ==========================\n" << std::endl;
    }

   
} // namespace