
#include "pch.h"
#include "AudioLoader.h"
#include "AudioProperties.h"

namespace Framework
{
    bool loadAudio(AudioProperties& audioProps) 
    {
        // Load sound using your Audio system
        FMOD::Sound* sound = GlobalAudio.UE_LoadSound(audioProps.name);
        if (!sound) 
        {
            std::cerr << "Failed to load sound: " << audioProps.name << std::endl;
            return false;
        }

        // Get duration in milliseconds
        unsigned int lengthMs = 0;
        if (sound->getLength(&lengthMs, FMOD_TIMEUNIT_MS) == FMOD_OK) 
        {
            audioProps.duration = lengthMs / 1000.0f; // Convert to seconds
        }
        return true;
    }
}