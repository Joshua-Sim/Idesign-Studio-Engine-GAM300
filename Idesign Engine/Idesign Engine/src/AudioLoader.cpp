
#include "pch.h"
#include "AudioLoader.h"
#include "AudioProperties.h"

namespace Framework
{
    bool loadAudio(AudioProperties& audioProps) 
    {
        // Load sound using your Audio system
        FMOD::Studio::EventDescription* eventDesc = GlobalAudio.UE_LoadEvent(audioProps.name);
        if (!eventDesc)
        {
            std::cerr << "Failed to load sound: " << audioProps.name << std::endl;
            return false;
        }

        // Get duration in milliseconds
        int lengthMs = 0;
        FMOD_RESULT res = eventDesc->getLength(&lengthMs);
        if (res == FMOD_OK && lengthMs > 0)
        {
            audioProps.duration = static_cast<float>(lengthMs) / 1000.0f; // Convert to seconds
        }
        else
        {
            audioProps.duration = 0.0f;
        }
        return true;
    }
}